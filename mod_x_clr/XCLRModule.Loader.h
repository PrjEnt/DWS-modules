// *
// * XCLRModule (mod_x_clr) :: Apache module for generate contents with CLR based dll (like CGI)
// * (C)2015 Dark Software / Dark Network Systems
// * http://n.dark-x.net/
// *
#define MOD_X_CLR_CLASS
#include "mod_x_clr.h"

// This file containts only CLI Commands.
#pragma managed

#using <mscorlib.dll>

using namespace System;
using namespace System::IO;
using namespace System::Reflection;

namespace XCLRModule{



	public interface class IModuleEntry {
		int Main(ModuleService^ MS);
	};



	public ref class ModuleLoader : MarshalByRefObject{

	internal:
		static Assembly^ LoadCLRLibrary(String^ FilePath){
			if (File::Exists(FilePath)) {
				FileStream^ FileLib = gcnew FileStream(FilePath, FileMode::Open, FileAccess::Read, FileShare::ReadWrite);
				array<Byte>^ MemLib = gcnew array<Byte> (FileLib->Length);
				FileLib->Read(MemLib, 0, (int)FileLib->Length);
				Assembly^ dll = Assembly::Load(MemLib);
				FileLib->Close();
				MemLib = nullptr;
				GC::Collect(0);
				return dll;
			}
			return nullptr;
		}

		static Assembly^ mod_x_clr_Loader(Object^ sender, ResolveEventArgs^ e) {
			AssemblyName^ AName = gcnew(AssemblyName)(e->Name);
			if(AName->Name == "mod_x_clr") {
				return Assembly::GetExecutingAssembly();
			}
			return nullptr;
		}

		static Assembly^ CLRLoader(Object^ sender, ResolveEventArgs^ e) {
			AssemblyName^ AName = gcnew(AssemblyName)(e->Name);
			return LoadCLRLibrary(e->Name);
		}

		IModuleEntry^ ME;

	public:
		ModuleLoader(){
		}

		void Load(String^ DLLPath){
			Assembly^ A = LoadCLRLibrary(DLLPath);
			Type^ T = A->GetType("XCLRModule.Startup");

			this->ME = (IModuleEntry^) Activator::CreateInstance(T);
		}

		int Main(ModuleService^ MS){
			return ME->Main(MS);
		}

	};



}