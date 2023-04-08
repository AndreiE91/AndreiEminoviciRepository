#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct parse_block {
    char SECT_NAME[14];
    int SECT_TYPE;
    int SECT_OFFSET;
    int SECT_SIZE;
} parse_block;

int check_sf_format(const char* file_path) {
    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        return 1;  // Error opening file
    }

    // Read the first 4 bytes of the file to check for the MAGIC field
    char magic[5]; // Extra space for the null string terminator, otherwise there will be problems in the docker test(memory outside is random gibberish),
                   // as opposed to local non-docker tests which apparently have null in between and this issue does not appear
    ssize_t bytes_read = read(fd, magic, 4);
    if (bytes_read != 4) {
        close(fd);
        return -1;  // Error reading file
    }
    magic[4] = '\0'; // Null terminate string to avoid unexpected behavior
    if (strncmp(magic, "hONA", 4) != 0) {
        close(fd);
        return 2;  // Invalid MAGIC field format
    }

    // Read the next 2 bytes of the file to check for the HEADER_SIZE field
    short header_size;
    bytes_read = read(fd, &header_size, 2);
    if (bytes_read != 2) {
        close(fd);
        return -1;  // Error reading file
    }

    // Read the next 4 bytes of the file to check for the VERSION field
    int version;
    bytes_read = read(fd, &version, 4);
    if (bytes_read != 4) {
        close(fd);
        return -1;  // Error reading file
    }
    if(version < 99 || version > 156) {
        close(fd);
        return 3;  // Invalid VERSION field format
    }

    // Read the next byte of the file to check for the NO_OF_SECTIONS field
    char no_of_sections;
    bytes_read = read(fd, &no_of_sections, 1);
    if (bytes_read != 1) {
        close(fd);
        return -1;  // Error reading file
    }
    if(no_of_sections < 2 || no_of_sections > 17) {
        close(fd);
        return 4;  // Invalid no_of_sections field format
    }

    parse_block *sections = malloc(no_of_sections * sizeof(parse_block)); // Allocate memory for an array to store section data

    for(int i = 0; i < no_of_sections; ++i) {
        // Read the next 14 bytes of the file to check for the SECT_NAME field
        char SECT_NAME[15];
        bytes_read = read(fd, SECT_NAME, 14);
        if (bytes_read != 14) {
            close(fd);
            free(sections);
            return -1;  // Error reading file
        }
        SECT_NAME[14] = '\0'; // Null terminate string to avoid unexpected behavior
        // Read the next 4 bytes of the file to check for the SECT_TYPE field
        int SECT_TYPE;
        bytes_read = read(fd, &SECT_TYPE, 4);
        if (bytes_read != 4) {
            close(fd);
            free(sections);
            return -1;  // Error reading file
        }
        if(SECT_TYPE != 48 && SECT_TYPE != 82 && SECT_TYPE != 81) {
            free(sections);
            return 5; //wrong sect_type
        }

        // Read the next 4 bytes of the file to check for the SECT_OFFSET field
        int SECT_OFFSET;
        bytes_read = read(fd, &SECT_OFFSET, 4);
        if (bytes_read != 4) {
            close(fd);
            free(sections);
            return -1;  // Error reading file
        }

        // Read the next 4 bytes of the file to check for the SECT_SIZE field
        int SECT_SIZE;
        bytes_read = read(fd, &SECT_SIZE, 4);
        if (bytes_read != 4) {
            close(fd);
            free(sections);
            return -1;  // Error reading file
        }
        strcpy(sections[i].SECT_NAME, SECT_NAME);
        sections[i].SECT_TYPE = SECT_TYPE;
        sections[i].SECT_OFFSET = SECT_OFFSET;
        sections[i].SECT_SIZE = SECT_SIZE;
    }
    
    printf("SUCCESS\n");
    printf("version=%d\n",version);
    printf("nr_sections=%d\n",no_of_sections);
    // Print all data about the sections from the sections array
    for(int i = 0; i < no_of_sections; ++i) {
        printf("section%d: %s %d %d\n", i + 1, sections[i].SECT_NAME, sections[i].SECT_TYPE, sections[i].SECT_SIZE);
    }

    free(sections); // Don't anger Valgrind
    close(fd);
    return 0;  // Success
}

int list_directory(const char* dir_path, int recursive, const char* size_greater, const char* name_ends_with) {
    DIR* dir = opendir(dir_path);
    if (!dir) {
        return 1;  // Invalid directory path
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Ignore hidden files and directories
        if (entry->d_name[0] == '.') {
            continue;
        }

        // Create the full path to the entry
        char* entry_path = malloc(strlen(dir_path) + strlen(entry->d_name) + 2);
        sprintf(entry_path, "%s/%s", dir_path, entry->d_name);

        // Get the stat information for the entry
        struct stat statbuf;
        int stat_result = stat(entry_path, &statbuf);
        if (stat_result != 0) {
            free(entry_path);
            closedir(dir);
            return 5;  // Error stating element
        }

        // Check if the entry meets the criteria
        int include_entry = 1;
        if (size_greater != NULL) {
            long size = strtol(size_greater, NULL, 10);
            if (statbuf.st_size <= size) {
                include_entry = 0;
            }
        }
        if (name_ends_with != NULL) {
            size_t name_len = strlen(entry->d_name);
            size_t suffix_len = strlen(name_ends_with);
            if (name_len < suffix_len || strcmp(entry->d_name + name_len - suffix_len, name_ends_with) != 0) {
                include_entry = 0;
            }
        }
        if (include_entry) {
            printf("%s\n", entry_path);
        }

        // If recursive is set and the entry is a directory, recurse into it
        if (recursive && S_ISDIR(statbuf.st_mode)) {
            int result = list_directory(entry_path, recursive, size_greater, name_ends_with);
            if (result != 0) {
                free(entry_path);
                closedir(dir);
                return result;
            }
        }

        free(entry_path);
    }

    closedir(dir);
    return 0;  // Success
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("ERROR\n");
        printf("Invalid command syntax.\n");
        return 1;
    }

    if (strcmp(argv[1], "variant") == 0) {
        printf("59950\n");
        return 0;
    } else if (strcmp(argv[1], "list") == 0) {
        // Parse command line options and parameters
        int recursive = 0;
        char* size_greater = NULL;
        char* name_ends_with = NULL;
        char* dir_path = NULL;
        for (int i = 2; i < argc; ++i) {
            if (strcmp(argv[i], "recursive") == 0) {
                recursive = 1;
            } else if (strncmp(argv[i], "size_greater=", 13) == 0) {
                size_greater = &argv[i][13];
            } else if (strncmp(argv[i], "name_ends_with=", 15) == 0) {
                name_ends_with = &argv[i][15];
            } else if (strncmp(argv[i], "path=", 5) == 0) {
                dir_path = &argv[i][5];
            }
        }

        // Call the list_directory function with the parsed options and parameters
        int result = list_directory(dir_path, recursive, size_greater, name_ends_with);
        if (result == 0) {
            printf("SUCCESS\n");
        } else {
            printf("ERROR\n");
            printf("invalid directory path\n");
        }
        return result;
    } else if (strcmp(argv[1], "parse") == 0) {
        // Parse command line options and parameters
        char* file_path = NULL;
        for (int i = 2; i < argc; ++i) {
            if (strncmp(argv[i], "path=", 5) == 0) {
                file_path = &argv[i][5];
            }
        }

        // Call the check_sf_format function with the parsed options and parameters
        int result = check_sf_format(file_path);
        if (result == 0) {
            return result; //printing of "SUCCESS" moved to function body
        } else if (result == 1) {
            printf("ERROR\n");
            printf("Failed to open file.\n");
        } else if (result == 2) {
            printf("ERROR\n");
            printf("wrong magic\n");
        } else if (result == 3) {
            printf("ERROR\n");
            printf("wrong version\n");
        } else if (result == 4) {
            printf("ERROR\n");
            printf("wrong sect_nr\n");
        } else if (result == 5) {
            printf("ERROR\n");
            printf("wrong sect_types\n");
        } else if (result == -1) {
            printf("ERROR\n");
            printf("Failed to read file\n");
        }
        return result;
    } else {
        printf("ERROR\n");
        printf("Invalid command syntax.\n");
        return 1;
    }
}
