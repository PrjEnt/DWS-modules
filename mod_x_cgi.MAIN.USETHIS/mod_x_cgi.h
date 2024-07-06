// *
// * XCGI Module (mod_x_cgi) :: Another implementation of 'mod_cgi' for Windows Embedded 7 Standard and later Windows / Windows Server
// * (C)2015-2018 Dark Network Systems
// * http://n.dark-x.net/
// *

#ifndef MOD_X_CGI_H
#define MOD_X_CGI_H 1

#include "../Include/Apache24Modules.h"

//Static Configuration
#define MaxPathSize 1024
#define MaxMemorySize 0xFFFF

//Constants
#define Pipe_Self 0
#define Pipe_Child 1

//Structs
typedef struct {
	apr_table_t* t;
	void *Block;			//Block Data
	int BlockSize;		//Max Block Size
	int ReadSize;

	char TKey[1024];	//Key
	char TValue[1024];	//Data Value
	int P;				//Pointer
	int IsData;			//Data Write Flag
	char Previous[2];	//Previous Character
} HeaderBlockManager;

typedef struct {
	int ReadSize;
	int SkipSize;
} OutputHeaderResults;

typedef struct {
	void* EnvBlock;
	int P;
	int MaxBlockSize;
} EnvBlockManager;


//Functions :: Utility
void None(void* Data);
extern inline int AtoI(const void *m);
extern inline void* x_palloc_withcheck(apr_pool_t *p, int size);
extern inline int FindCRLFEOF(void *Data, int DataSize);
extern inline int ReplaceChar(char Find, char Replace, void *Data, int DataSize);
extern inline int BackSlashToSlash(void *Data, int DataSize);
extern inline int SlashToBackSlash(void *Data, int DataSize);
extern inline int GetScriptExe(apr_pool_t *p, const char* ScriptPath, char** ExePathOut, int ScriptPathSize);
extern inline int AddHeadersFromBlock(HeaderBlockManager* HBM);
extern inline int AddEnvBlock(EnvBlockManager* EBM, const char* Key, const char* Value);
extern inline int AddEnvBlockHTTP(EnvBlockManager* EBM, const char* Key, const char* Value);
extern inline int FindFinal(char Find, void* Data, int DataSize);

//Functions :: Services
extern inline int BuildEnvBlock(request_rec *r, EnvBlockManager *EBM, int FileNameLastSlash, char* ENV_SR);
extern inline int PassPOSTToScript(request_rec *r, HANDLE* HStdIn);
extern inline int OutputHeaders(request_rec *r, void* Data, int DataSize, HANDLE *HStdOut, OutputHeaderResults* OHR);
extern inline int PassContentsToClient(request_rec *r, void* Data, int DataSize, HANDLE* HStdOut, OutputHeaderResults *OHR);
extern inline int PreapreProcess(request_rec *r, STARTUPINFO* si, HANDLE *HStdOut, HANDLE *HStdIn, char **ScriptCommand);
extern inline int CreateCGIProcess(request_rec *r, STARTUPINFO* si, PROCESS_INFORMATION *pi, char *ScriptCommand, int FileNameLastSlash, EnvBlockManager *EBM);
#endif //MOD_X_CGI_H
