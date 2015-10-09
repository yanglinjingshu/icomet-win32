

#ifndef chatserv_file_h
#define chatserv_file_h
#include  <io.h>
#include  <stdio.h>
#include  <stdlib.h>


static inline bool is_file(const char *filename){
	if ((_access(filename, 0)) != -1)
	{
		printf("File %s exists\n", filename);
		/* Check for write permission */
		if ((_access(filename, 2)) != -1)
		{
			printf("File %s has write permission\n", filename);

		}
		return true;
	}
	else
	{
		return false;
	}
}

//#include <sys/types.h>
//#include <sys/stat.h>
////#include <unistd.h>
//
//#define S_ISREG(m) (((m) & 0170000) == (0100000))   
//#define S_ISDIR(m) (((m) & 0170000) == (0040000))  
//
//static inline bool file_exists(const char *filename){
//	struct stat st;
//	return stat(filename, &st) == 0;
//}
//
//static inline bool is_dir(const char *filename){
//	struct stat st;
//	if (stat(filename, &st) == -1){
//		return false;
//	}
//	return (bool)S_ISDIR(st.st_mode);
//}
//
//static inline bool is_file(const char *filename){
//	struct stat st;
//	if (stat(filename, &st) == -1){
//		return false;
//	}
//	return (bool)S_ISREG(st.st_mode);
//}

#endif
