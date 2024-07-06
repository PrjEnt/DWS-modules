// *
// * XCGI Module (mod_x_cgi) :: Another implementation of 'mod_cgi' for Windows Embedded 7 Standard and later Windows / Windows Server
// * (C)2015-2018 Dark Network Systems
// * http://n.dark-x.net/
// *
#include "mod_x_cgi.h"

inline int PreapreProcess(request_rec *r, STARTUPINFO* si, HANDLE *HStdOut, HANDLE *HStdIn, char **ScriptCommand){
	
	int R; //TEMP Pointer
	int ScriptExeSize;
	int ScriptCommandSize;
	char *ScriptExe;
	char *TEMP;
	SECURITY_ATTRIBUTES sa = {0};

	int ScriptFileNameSize = strlen(r->filename);

	//
	ScriptExeSize = GetScriptExe(r->pool, r->filename, &ScriptExe, ScriptFileNameSize);
	if(ScriptExeSize < 0){
		Log(r, APLOG_WARNING, "mod_x_cgi", "Failed to parse first line of script (%i): [%s]", -ScriptExeSize, r->filename);
		return -ScriptExeSize;
	}

	//Initialize SECURITY_ATTRIBUTES
	sa.nLength = sizeof(SECURITY_ATTRIBUTES); 
	sa.bInheritHandle = 1; 

	//Create Pipe
	if(! CreatePipe(&HStdOut[Pipe_Self], &HStdOut[Pipe_Child], &sa, 0)){ //Self <--> Child
		Log(r, APLOG_ERR, "mod_x_cgi", "Failed to create STDOUT pipe.");
		return 500;
	}
	if(! CreatePipe(&HStdIn[Pipe_Child], &HStdIn[Pipe_Self], &sa, 0)){ //Child <--> Self
		Log(r, APLOG_ERR, "mod_x_cgi", "Failed to create STDIN pipe.");
		return 500;
	}

	//Initailize STARTUPINFO
	si->cb = sizeof(STARTUPINFO);
	si->dwFlags = STARTF_USESTDHANDLES;
	si->hStdOutput = HStdOut[Pipe_Child];
	si->hStdInput = HStdIn[Pipe_Child];

	ScriptCommandSize = ScriptExeSize + 4 + 4; //4 = [ ""\0]
	(*ScriptCommand) = (char*) x_palloc_withcheck(r->pool, ScriptExeSize);
	if( (*ScriptCommand) == 0 ){
		Log(r, APLOG_ERR, "mod_x_cgi", "Exceed MaxMemorySize(%i): %i", MaxMemorySize, ScriptCommandSize );
		return 414; //URI Too long
	}

	ZeroMemory((*ScriptCommand), MaxPathSize);

	memcpy((*ScriptCommand), ScriptExe, ScriptExeSize); //Copy
	R = ScriptExeSize; //Pointer set
	(*ScriptCommand)[R++] = ' '; //Arg
	(*ScriptCommand)[R++] = '"'; //Script Filename Start
	memcpy( ((*ScriptCommand)+R) , r->filename, ScriptFileNameSize); //Copy Script FileName
	R += ScriptFileNameSize; //Pointer++
	(*ScriptCommand)[R++] = '"'; //ScriptFileName End
	return 0;
}