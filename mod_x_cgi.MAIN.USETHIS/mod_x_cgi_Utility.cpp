// *
// * XCGI Module (mod_x_cgi) :: Another implementation of 'mod_cgi' for Windows Embedded 7 Standard and later Windows / Windows Server
// * (C)2015-2018 Dark Network Systems
// * http://n.dark-x.net/
// *
#include "mod_x_cgi.h"

void None(void *Data){}


inline int AtoI(const void *m){
	const char* s = (const char*) m;
	int r = 0;
	while(true){
		if (*s >= '0' && *s <= '9') {
			r = (int) (r * 10) + (*s - '0');
			s++;
		} else  {	
			break;
		}
	}
	return r;
}

inline void* x_palloc_withcheck(apr_pool_t *p, int size) {
	if( size <= MaxMemorySize ){
		return apr_palloc(p, size);
	}
	return 0;
}

inline int FindCRLFEOF(void *Data, int DataSize){
	char *DataC = (char*)Data;
	int i = 0;
	for(i = 0; i < DataSize; i++){
		switch(DataC[i]){
		case 0x0D: 
		case 0x0A:
		case 0x00:
			return i;
		}
	}
	return -1;
}

inline int ReplaceChar(char Find, char Replace, void *Data, int DataSize){

	char *DataC = (char*)Data;
	int i = 0;
	for(i = 0; i < DataSize; i++){
		if(DataC[i] == Find){
			DataC[i] = Replace;
			continue;
		}
		switch(DataC[i]){
		case ' ': 
		case 0x0D: 
		case 0x0A:
		case 0x00:
			return i;
			break;
		}
	}
	return i;
}

inline int BackSlashToSlash(void *Data, int DataSize){
	return ReplaceChar('\\', '/', Data, DataSize);;
}

inline int SlashToBackSlash(void *Data, int DataSize){
	return ReplaceChar('/', '\\', Data, DataSize);;
}


inline int GetScriptExe(apr_pool_t *p, const char* ScriptPath, char** ExePathOut, int ScriptPathSize){
	//Return:	+: OK: size of ExePathOut
	//			-: ERROR: HTTP_STATUS_CODE (CANT_CONTINUE)

	HANDLE H;
	char SF[1024];

	char* ScriptFile = SF;//(char*) apr_palloc(p, MaxPathSize);
	unsigned int *ScriptFile4 = (unsigned int*) ScriptFile; //Pointer[4]

	int ReadSize = 0;	

	H = CreateFile( (char*)ScriptPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
	if(H == 0){
		return -404;
	}

	ReadFile(H, ScriptFile, MaxPathSize, (DWORD*)&ReadSize, 0);
	CloseHandle(H);

	if( ( ScriptFile4[0] & 0x00FFFFFF ) == 0x00BFBBEF ){
		ScriptFile += 3;
	}

	if( ! memcmp(ScriptFile, "MZ", 2)){
		if(ScriptPathSize == -1){
			ReadSize = strlen(ScriptPath);
		} else {
			ReadSize = ScriptPathSize;
		}

		*ExePathOut = (char*) x_palloc_withcheck(p, ReadSize + 4);
		if( (*ExePathOut) == 0 ){ return -414; }
		ZeroMemory(*ExePathOut, ReadSize + 4);

		memcpy(*ExePathOut, ScriptPath, ReadSize);
		//*( (*ExePathOut) + ReadSize) = 0x00;
		return ReadSize;
	}

	if( ! memcmp(ScriptFile, "#!", 2)){
		ScriptFile += 2;
		ReadSize = FindCRLFEOF(ScriptFile, ReadSize);
		SlashToBackSlash(ScriptFile, ReadSize);

		*ExePathOut = (char*) x_palloc_withcheck(p, ReadSize + 4);
		if( (*ExePathOut) == 0 ){ return -414; }
		ZeroMemory(*ExePathOut, ReadSize + 4);

		memcpy(*ExePathOut, ScriptFile, ReadSize);
		//*( (*ExePathOut) + ReadSize) = 0x00;
		return ReadSize;
	}

	return -500;
}


inline int AddHeadersFromBlock(HeaderBlockManager* HBM){

	char *DataC = (char*) HBM->Block;

	int i = 0;
	for( i=0; (i < HBM->ReadSize); i++){
		HBM->Previous[2] = HBM->Previous[1];
		HBM->Previous[1] = HBM->Previous[0];
		HBM->Previous[0] = DataC[i];

		switch( DataC[i] ){
		case ':':
			if( ! (HBM->IsData) ){ //If IsData = true. This ":" is string.
				HBM->TKey[(HBM->P)++] = 0x00;
				HBM->IsData = 1;
				HBM->P = 0;
				i++; //Space after :
				continue;
			}
			break;

		case 0x0D: //CR
			continue;

		case 0x0A: //LF
			if( (HBM->Previous[1] == 0x0A) ){ //[*, 0x0A, 0x0A] = End
				return i;
			}
			if( (HBM->Previous[2] == 0x0A) ){ //[0x0A, 0x0D, 0x0A] = End
				return i;
			}

			if( ! (HBM->IsData) ){ //If IsData = true, Error.
				continue;
			}

			HBM->TValue[(HBM->P)++] = 0x00;

			apr_table_add(HBM->t, HBM->TKey, HBM->TValue);

			HBM->IsData = 0;
			HBM->P = 0;
			continue;
		case 0x00:
			return i;
		}

		if ( ! HBM->IsData ){ //Key
			HBM->TKey[(HBM->P)++] = DataC[i];
		} else { //Data
			HBM->TValue[(HBM->P)++] = DataC[i];
		}

	}
	return -1;
}

void ClearEnvBlock(EnvBlockManager *EBM){
}

inline int AddEnvBlock(EnvBlockManager* EBM, const char* Key, const char* Value){
	int i = 0;
	int StartP = 0;

	char *BlockC = (char*) EBM->EnvBlock;

	if(Key == 0 || Value == 0){
		return 0;
	}

	if(*Key == 0){
		return 0;
	}

	if( (EBM->MaxBlockSize - EBM->P) > 4 ){ //Reserve x2 null termination
		StartP = EBM->P;
		for(i = 0; i < (EBM->MaxBlockSize - StartP); i++){
			if(*Key == 0){break;}
			BlockC[(EBM->P)++] = *Key++;
		}

		BlockC[(EBM->P)++] = '=';

		StartP = EBM->P;
		for(i = 0; i < (EBM->MaxBlockSize - StartP); i++){
			if(*Value == 0){break;}
			BlockC[(EBM->P)++] = *Value++;
		}

		BlockC[(EBM->P)++] = 0;
		BlockC[(EBM->P)] = 0;
	}

	return 0;
}

inline int AddEnvBlockHTTP(EnvBlockManager* EBM, const char* Key, const char* Value){
	char *BlockC = (char*) EBM->EnvBlock;
	if( (EBM->MaxBlockSize - EBM->P) > (5+2) ){ //Reserve x2 null termination
		BlockC[(EBM->P)++] = 'H';
		BlockC[(EBM->P)++] = 'T';
		BlockC[(EBM->P)++] = 'T';
		BlockC[(EBM->P)++] = 'P';
		BlockC[(EBM->P)++] = '_';
	}
	BlockC[(EBM->P)] = 0;
	BlockC[(EBM->P)+1] = 0;
	return AddEnvBlock(EBM, Key, Value);
}

inline int FindFinal(char Find, void* Data, int DataSize){
	int i = 0;
	int LastF = -1;
	char *DataC = (char*) Data;

	for(i = 0; i < DataSize; i++){
		if(DataC[i] == Find){
			LastF = i;
			continue;
		}
		if(DataC[i] == 0){
			break;
		}
	}
	return LastF;
}

