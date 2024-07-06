// *
// * XCGI Module (mod_x_cgi) :: Another implementation of 'mod_cgi' for Windows Embedded 7 Standard and later Windows / Windows Server
// * (C)2015-2018 Dark Network Systems
// * http://n.dark-x.net/
// *
#include "mod_x_cgi.h"

inline int CreateCGIProcess(request_rec *r, STARTUPINFO* si, PROCESS_INFORMATION *pi, char *ScriptCommand, int FileNameLastSlash, EnvBlockManager *EBM){
	int R = 0; //Pointer
	char* TEMP;

	//Build Current Directory
	R = FileNameLastSlash;
	TEMP = (char*) x_palloc_withcheck(r->pool, (FileNameLastSlash + 2) );
	memcpy(TEMP, r->filename, R);
	TEMP[R++] = 0;

	if(!CreateProcess(0, ScriptCommand, 0, 0, 1, CREATE_NO_WINDOW | NORMAL_PRIORITY_CLASS, EBM->EnvBlock, TEMP, si, pi)){
		Log(r, APLOG_ERR, "mod_x_cgi", "Failed to exec script (%i): [%s]", GetLastError(), ScriptCommand);
		ap_rputs("Failed to exec script.", r);
		return 200;
//		return 500;
	}
	return 0;
}