// gcc -Wall -Werror a3.c -o a3 -lrt
// python3 tester.py --docker
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>

#define RESP_PIPE "RESP_PIPE_59950"
#define REQ_PIPE "REQ_PIPE_59950"
#define BUFFER_SIZE 256

void writep(int fd, const void *buf, size_t n) {
    if(write(fd, buf, n) == -1) {
        perror("write pipe error");
        exit(1);
    }
}

int main() {
    // If the named pipe already exists, remove it to ensure data consistency
    unlink(RESP_PIPE);
    // Create the named pipe
    if (mkfifo(RESP_PIPE, 0666) == -1) {
        printf("ERROR\ncannot create the response pipe\n");
        exit(EXIT_FAILURE);
    }
    // Open the created named pipe for writing
    int fd_write = open(RESP_PIPE, O_RDWR);
    if (fd_write == -1) {
        perror("Failed to open the response pipe for writing");
        exit(EXIT_FAILURE);
    }
    // Open the provided named pipe for reading
    int fd_read = open(REQ_PIPE, O_RDONLY);
    if (fd_read == -1) {
        printf("ERROR\ncannot open the request pipe\n");
        exit(EXIT_FAILURE);
    }

    // Perform write operation on the created named pipe
    char *request = "START!";
    writep(fd_write, request, strlen(request));
    printf("SUCCESS\n");

    // These declarations will be later assigned their respective values. This is 

    int shm_fd; // Shared memory file descriptor
    unsigned int shm_size; // Shared memory size
    const char* shm_name; // Shared memory object name
    char* shm_ptr = NULL; // Shared memory address

    char* file_name; // Memory mapped file name
    int mm_fd = -1; // Memory mapped file descriptor
    char* mapped_ptr = NULL; // Start address of the mapped memory
    struct stat file_stat; // Structure which holds information about the file which will be memory mapped. I am interested in its size.

    char loop = 1; // Allocate 1 byte for a loop flag
    // Enter success case loop
    while(loop) {
        // Read data from the pipe until it is empty
        char buffer[BUFFER_SIZE];
        memset(buffer,0,sizeof(buffer)); // Zero out buffer to avoid data interference and inconsistency

        usleep(2000); // This seems to solve inconsistencies related to the communication between the tester and my program.
        // Sometimes the tester is not synchronized with my program when it sends or receives requests, and this alone is driving me crazy for the past 15 hours.
        // Because of this, sometimes I get a high grade, sometimes I get a lower grade because it reads MESSAGE instead of MESSAGE! at some point
        // and on the next read it reads the missed ! together with the new string. I tried to create a while loop to avoid partial reads, this approach
        // now creates the issue where the whole program blocks due to a read on an empty pipe at some point when the loop should be exited and, again, this is not good.
        // Then, maybe if I removed read blocking things would get better, right? Wrong. If read does not block, then the while loop gets executed even when the buffer is empty
        // and does not wait for the tester whatsoever. I cannot determine 100% why this behavior is the way it is, and I feel I already lost a lot of time side-tracked off the main scope
        // of this assignment which is why for the moment at least, I will proceed with this "patch-up" solution which I am happy solves most of the issues.
        // UPDATE: Seems like while loop still gets executed more than I expect it to, but it does nothing until a message arrives on the pipe.
        // Last 10 tests all behaved the same, which makes me happy. I am going to focus on the next requirements and not touch this if it works.
        ssize_t bytesRead = read(fd_read, buffer, BUFFER_SIZE);
        //printf("Buffer: %s\n", buffer);

        // Handle the request read from the request pipe
        if(strncmp(buffer, "VARIANT!", bytesRead) == 0) {
            char *response = "VARIANT!VALUE!";
            writep(fd_write, response, strlen(response));
            int variant = 59950;
            writep(fd_write, &variant, sizeof(int));
        } else if(strncmp(buffer, "CREATE_SHM!", strlen("CREATE_SHM!")) == 0) {
            shm_size = *(unsigned int*)(buffer + strlen("CREATE_SHM!"));
            shm_name = "/dy4nuB";

            char* error_response = "CREATE_SHM!ERROR!";

            // Create the shared memory object
            shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0664);
            if (shm_fd == -1) {
                perror("shm_open");
                writep(fd_write, error_response, strlen(error_response));
                continue;
            }

            // Adjust the size of the shared memory object
            if (ftruncate(shm_fd, shm_size) == -1) {
                perror("ftruncate");
                writep(fd_write, error_response, strlen(error_response));
                continue;
            }

            // Map the shared memory object into the virtual address space
            //printf("Size = %d\n", shm_size);
            shm_ptr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
            if (shm_ptr == MAP_FAILED) {
                perror("mmap");
                writep(fd_write, error_response, strlen(error_response));
                continue;
            }

            char* response = "CREATE_SHM!SUCCESS!";
            writep(fd_write, response, strlen(response));

        } else if(strncmp(buffer, "WRITE_TO_SHM!", strlen("WRITE_TO_SHM!")) == 0) {
            unsigned int offset = *(unsigned int*)(buffer + strlen("WRITE_TO_SHM!"));
            unsigned int value = *(unsigned int*)(buffer + strlen("WRITE_TO_SHM!") + sizeof(unsigned int));
            //printf("Offset: %d Value: %d\n", offset, value);
            char* error_response = "WRITE_TO_SHM!ERROR!";
            char* response = "WRITE_TO_SHM!SUCCESS!";
            // Validate if there is enough space to write the value bytes at the offset
            if(offset < 3732669 - sizeof(unsigned int)) {
                // Modify the value at the specified offset
                *(int*)(shm_ptr + offset) = value;
                writep(fd_write, response, strlen(response));
            } else {
                writep(fd_write, error_response, strlen(error_response));
            }

        } else if(strncmp(buffer, "MAP_FILE!", strlen("MAP_FILE!")) == 0) {
            char* error_response = "MAP_FILE!ERROR!";
            char* response = "MAP_FILE!SUCCESS!";

            file_name = (buffer + strlen("MAP_FILE!")); // Read file name from the buffer at the offset of the number of bytes the request message occupies
            file_name[strlen(file_name) - 1] = '\0'; // Remove ! character from end of file name received
            //printf("File name: %s\n", file_name);
            // Open the file
            mm_fd = open(file_name, O_RDONLY);
            if (mm_fd == -1) {
                perror("open");
                writep(fd_write, error_response, strlen(error_response));
                continue;
            }

            // Get the file size
            if (fstat(mm_fd, &file_stat) == -1) {
                perror("fstat");
                writep(fd_write, error_response, strlen(error_response));
                continue;
            }

            // Map the file into memory
            mapped_ptr = mmap(NULL, file_stat.st_size, PROT_READ, MAP_SHARED, mm_fd, 0);
            if (mapped_ptr == MAP_FAILED) {
                perror("mmap");
                writep(fd_write, error_response, strlen(error_response));
                continue;
            }
            writep(fd_write, response, strlen(response));

        } else if(strncmp(buffer, "READ_FROM_FILE_OFFSET!", strlen("READ_FROM_FILE_OFFSET!")) == 0) {
            unsigned int offset = *(unsigned int*)(buffer + strlen("READ_FROM_FILE_OFFSET!"));
            unsigned int no_of_bytes = *(unsigned int*)(buffer + strlen("READ_FROM_FILE_OFFSET!") + sizeof(unsigned int));
            //printf("Offset: %d Num bytes: %d Size: %ld\n", offset, no_of_bytes, file_stat.st_size);
            char* error_response = "READ_FROM_FILE_OFFSET!ERROR!";
            char* response = "READ_FROM_FILE_OFFSET!SUCCESS!";
            
            
            // Apply principle of inversion to handle the error case first
            if(mapped_ptr == NULL || mm_fd == -1 || offset + no_of_bytes > file_stat.st_size) {
                writep(fd_write, error_response, strlen(error_response));
            } else {

                // printf("Mapped ptr: %p\n", mapped_ptr);
                // printf("Mapped ptr data: %s\n", (char*)(mapped_ptr));
                // Read number of bytes from offset in the shared memory and copy them at the beginning of the shared memory region
                for(int i = 0; i < no_of_bytes; ++i) {
                    shm_ptr[i] = mapped_ptr[offset + i];
                }

                // Announce the success case
                writep(fd_write, response, strlen(response));
            }

        } else if(strncmp(buffer, "READ_FROM_FILE_SECTION!", strlen("READ_FROM_FILE_SECTION!")) == 0) {
            unsigned int section_no = *(unsigned int*)(buffer + strlen("READ_FROM_FILE_SECTION!"));
            unsigned int offset = *(unsigned int*)(buffer + strlen("READ_FROM_FILE_SECTION!") + sizeof(unsigned int));
            unsigned int no_of_bytes = *(unsigned int*)(buffer + strlen("READ_FROM_FILE_SECTION!") + 2 * sizeof(unsigned int));
            printf("Section_no: %d Offset: %d Num bytes: %d  Size: %ld\n", section_no, offset, no_of_bytes, file_stat.st_size);
            char* error_response = "READ_FROM_FILE_SECTION!ERROR!";
            char* response = "READ_FROM_FILE_SECTION!SUCCESS!";

            // MAGIC: 4
            // HEADER_SIZE: 2
            // VERSION: 4
            // NO_OF_SECTIONS: 1
            // SECTION_HEADERS: NO_OF_SECTIONS * sizeof(SECTION_HEADER)
            // SECTION_HEADER: 14 + 4 + 4 + 4
            // SECT_NAME: 14
            // SECT_TYPE: 4
            // SECT_OFFSET: 4
            // SECT_SIZE: 4

            // • The MAGIC’s value is “hONA”.
            // • VERSION’s value must be between 99 and 156, including that values.
            // • The NO OF SECTIONS’s value must be between 2 and 17, including that values.
            // • Valid SECT TYPE’s values are: 48 82 81 .

            // Fetch file data from its header and check its validity
            char magic[5]; memcpy(magic, mapped_ptr, 4);
            magic[4] = '\0'; // Null terminate string
            if(strcmp(magic, "hONA") != 0) {
                printf("File header error: Invalid magic field\n");
                writep(fd_write, error_response, strlen(error_response));
                continue;
            }
            short header_size = 0; memcpy(&header_size, mapped_ptr + 4, 2);
            int version = 0; memcpy(&version, mapped_ptr + 6, 4);
            if(version < 99 || version > 156) {
                printf("File header error: Invalid version field\n");
                writep(fd_write, error_response, strlen(error_response));
                continue;
            }
            char no_of_sections = 0; memcpy(&no_of_sections, mapped_ptr + 10, 1);
            if(no_of_sections < 2 || no_of_sections > 17) {
                printf("File header error: Invalid no_of_sections field\n");
                writep(fd_write, error_response, strlen(error_response));
                continue;
            }
            char section_names[no_of_sections][15];
            int sect_types[no_of_sections];
            int sect_offsets[no_of_sections];
            int sect_sizes[no_of_sections];
            char broken = 0; // Flag needed for double loop break
            for(int i = 0; i < no_of_sections; ++i) {
                memcpy(&section_names[i], mapped_ptr + 11 + i * 26, 14);
                section_names[i][14] = '\0';
                memcpy(&sect_types[i], mapped_ptr + 11 + i * 26 + 14, 4);
                if(sect_types[i] != 48 && sect_types[i] != 82 && sect_types[i] != 81) {
                    printf("File header error: Invalid sect_types field\n");
                    writep(fd_write, error_response, strlen(error_response));
                    broken = 1;
                    break;
                }
                memcpy(&sect_offsets[i], mapped_ptr + 11 + i * 26 + 18, 4);
                memcpy(&sect_sizes[i], mapped_ptr + 11 + i * 26 + 22, 4);
            }
            if(broken) {
                continue;
            }

            if(mapped_ptr == NULL || mm_fd == -1 || sect_offsets[section_no - 1] + offset + no_of_bytes > file_stat.st_size || section_no < 1 || section_no > no_of_sections) {
                writep(fd_write, error_response, strlen(error_response));
            }

            // Debug print file's header
            printf("MAGIC: %s  HEADER_SIZE: %d  VERSION: %d  NO_OF_SECTIONS: %d\n", magic, header_size, version, no_of_sections);
            for(int i = 0; i < no_of_sections; ++i) {
                printf("section_names[%d] = %s\n", i, section_names[i]);
                printf("sect_types[%d] = %d\n", i, sect_types[i]);
                printf("sect_offsets[%d] = %d\n", i, sect_offsets[i]);
                printf("sect_sizes[%d] = %d\n\n", i, sect_sizes[i]);
            }

            //Read and copy read bytes into asked location            
            for(int i = 0; i < no_of_bytes; ++i) {
                shm_ptr[i] = mapped_ptr[sect_offsets[section_no - 1] + offset + i];
            }

            // Announce the success case
            writep(fd_write, response, strlen(response));
        } else if(strncmp(buffer, "EXIT!", bytesRead) == 0) {
            loop = 0;
            break;
        }
    }

    // Try to close anything which could have been used so far

    // Close the file descriptors
    close(fd_write);
    close(fd_read);


    // Remove the named pipe
    unlink(RESP_PIPE);

    // Unmap the shared memory object
    munmap(shm_ptr, shm_size);

    // Close the shared memory file descriptor
    close(shm_fd);

    // Remove the shared memory object
    shm_unlink(shm_name);

    // Unmap the file from memory
    munmap(mapped_ptr, file_stat.st_size);

    // Close the file
    close(mm_fd);

    return 0;
}