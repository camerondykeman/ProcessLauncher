/**
    Concurrency Assignment 1
    Purpose: Launches applications either serially or concurrently by launch-group.

    @author Cameron Dykeman
    @version 1.0 23/05/2013
*/

#include <iomanip>
#include <cassert>
#include <iostream>
#include <thread>
#include <future>
#include <string>
#include <Windows.h>
#include <sstream>
#include <fstream>
#include <map>
using namespace std;

//Class for holding all data related to a launchable process
class Launchable{
public:
	virtual ~Launchable(){}
	int launchGroup;
	string path;
	string params;
	HANDLE	hProcess;
	DWORD	threadID;
	DWORD	exitCode;
	SYSTEMTIME kTime;
	SYSTEMTIME uTime;
	Launchable(int launchGroup, string path, string params) : launchGroup(launchGroup), path(path), params(params), threadID(0), hProcess(0), exitCode(0){}
};

/**
    Reads in instructions from a file and parses those instructions into a map of launch groups.

    @param filePath The directory location of the process to be launched.
	@param launchGroups A map of all process groups to be launched.
    @return Exit code.
*/
int getLaunchGroupsFromFile(string filePath, map<int, vector<Launchable>> &launchGroups){
	//open file
	ifstream textFile( filePath.c_str() );
	if ( ! textFile.is_open() )	
	{
		cout << "Error: failed to open directory" << endl;
		return -1;
	}
	
	//read in line by line
	string line;				//current live being read
	int execNum = 0;				//the execution group for the given line
	string execPath;			//the file path for the given line
	string execParams;			//the parameters for the given line

	while(getline(textFile, line)){
		//loop through elements in line
		//isolate each element
		istringstream lineStream(line);
		string token;
		int count = 0;
		while(getline(lineStream, token, ',')){
			switch(count){
			case 0:	//launch group
				execNum = stoi(token);
				++count;
				break;
			case 1:	//program name
				execPath = token;
				++count;
				break;
			case 2:	//cmd params
				execParams = token;
				count = 0;
				break;
			}
		}
		//combine items into Launchable
		Launchable launchable = Launchable(execNum, execPath, execParams);
		std::map<int, vector<Launchable>>::iterator iter = launchGroups.find(execNum);
		//add to map
		//already exists - get vector from map, add, and reinsert
		if (iter != launchGroups.end())
		{
			vector<Launchable> addToGroup = iter->second;
			addToGroup.push_back(launchable);
			iter->second = addToGroup;
		}
		//doesn't exist - make new vector, add, insert
		else
		{
			vector<Launchable> newGroup;
			newGroup.push_back(launchable);
			launchGroups[execNum] = newGroup;
		}

	}
	return 0;
}

/**
    Launches processes in the launch group concurrently.

    @param launchGroup The group of processes to be launched concurrently
    @return Exit code.
*/
int launchGroupConcurrently(vector<Launchable>& launchGroup){

	//loop through launchables in launchGroup
	for( Launchable& launchable : launchGroup ){

		wstring application(launchable.path.begin(), launchable.path.end());
		wstring params(launchable.params.begin(), launchable.params.end());
		wstring command = application + L" " + params;

		STARTUPINFO sinfo = {0};
		sinfo.cb = sizeof(STARTUPINFO);
		PROCESS_INFORMATION pi = {0};

		try{
			unsigned long const CP_MAX_COMMAND_LINE = 32768;
			wchar_t* commandLine = new wchar_t[CP_MAX_COMMAND_LINE];
			wcsncpy_s(commandLine, CP_MAX_COMMAND_LINE, command.c_str(), command.size()+1);
			CreateProcess( NULL, //app name is in the command line params
				commandLine,	 //full command line (appname + params)
				NULL,			 //process security
				NULL,			 //main thread security
				false,
				0, 
				NULL,			 //
				NULL, 
				&sinfo, 
				&pi );

			launchable.hProcess = pi.hProcess;
			delete [] commandLine;
		}catch(std::bad_alloc){
			wcerr << L"Insufficient memory to launch application" << endl;
			return -1;
		}
		cout << "2" << this_thread::get_id << endl;
	}

	//get handles for all threads
	vector<HANDLE> handles;
	for( Launchable launchable : launchGroup ){
		handles.push_back( launchable.hProcess );
	}
	//wait for threads to finish
	WaitForMultipleObjects( handles.size(), handles.data(), TRUE, INFINITE );

	//get thread exit codes
	for( Launchable& launchable : launchGroup )
		GetExitCodeProcess( launchable.hProcess, &launchable.exitCode );
	
	//Get run times
	for(Launchable& launchable : launchGroup){
		FILETIME creationTime, exitTime, kernelTime, userTime;
		GetProcessTimes(launchable.hProcess, &creationTime, &exitTime, &kernelTime, &userTime);
		FileTimeToSystemTime(&kernelTime, &launchable.kTime);
		FileTimeToSystemTime(&userTime, &launchable.uTime);
	}
	
	//close thread handles
	for( Launchable launchable : launchGroup )
		CloseHandle( launchable.hProcess );
	
	return 0;
}

/**
    Launches processes in the launch group serially.

    @param launchGroup The group of processes to be launched serially.
    @return Exit code.
*/
int launchGroupSerially(vector<Launchable>& launchGroup){

	Launchable& launchable = launchGroup.front();

	//get cmd input
	wstring application(launchable.path.begin(), launchable.path.end());
	wstring params(launchable.params.begin(), launchable.params.end());
	wstring command = L"\"" + application + L"\" " + params;

	STARTUPINFO sinfo = {0};
	sinfo.cb = sizeof(STARTUPINFO);
	PROCESS_INFORMATION pi = {0};

	try{
		unsigned long const CP_MAX_COMMAND_LINE = 32768;
		wchar_t* commandLine = new wchar_t[CP_MAX_COMMAND_LINE];
		wcsncpy_s(commandLine, CP_MAX_COMMAND_LINE, command.c_str(), command.size()+1);

		CreateProcess( NULL, //app name is in the command line params
			commandLine,	 //full command line (appname + params)
			NULL,			 //process security
			NULL,			 //main thread security
			false,
			0, 
			NULL,			 
			NULL, 
			&sinfo, 
			&pi );
		launchable.hProcess = pi.hProcess;
		delete [] commandLine;
	}catch(std::bad_alloc){
		wcerr << L"Insufficient memory to launch application" << endl;
		return -1;
	}
		
	//wait for threads to finish
	WaitForSingleObject( launchable.hProcess, INFINITE );

	//get thread exit codes
	GetExitCodeProcess( launchable.hProcess, &launchable.exitCode );
	
	//Get run times
	FILETIME creationTime, exitTime, kernelTime, userTime;
	GetProcessTimes(launchable.hProcess, &creationTime, &exitTime, &kernelTime, &userTime);
	FileTimeToSystemTime(&kernelTime, &launchable.kTime);
	FileTimeToSystemTime(&userTime, &launchable.uTime);
	
	//close thread handles
	CloseHandle( launchable.hProcess );

	return 0;
}

/**
    Prints out the results from the processes run.

    @param launchGroup The groups launched.
    @return Exit code.
*/
int printReport(map<int, vector<Launchable>> launchGroups){
	//loop through launch groups
	bool failed = false;
	cout << endl << "Succeeded:" << endl;
	cout << setw(2) << "#" << setw(5) << "kT" << setw(5) << "uT" << setw(5) << "eC" << setw(20) << "Path" << setw(20) << "Parameters" << endl;
	cout << setw(57) << setfill('-') << "-" << endl;
	map<int, vector<Launchable>>::iterator launchIter;
	//loop and print successes
	for(launchIter = launchGroups.begin(); launchIter != launchGroups.end(); launchIter++){
		//loop through processes
		for(Launchable launchable : launchIter->second){
			if(launchable.exitCode!=0){
				cout << setw(2) << setfill(' ') << launchable.launchGroup << setw(5) << launchable.kTime.wMilliseconds << setw(5) << launchable.uTime.wMilliseconds << setw(5) << launchable.exitCode << setw(20) << launchable.path << setw(20) << launchable.params << endl;
			}
			else{
				failed = true;
			}
		}
	}
	if(failed==true){
		//loop and print failures
		cout << endl << "Failed:" << endl;
		cout << setw(2) << "#" << setw(5) << "eC" << setw(20) << "Path" << setw(20) << "Parameters" << endl;
		cout << setw(57) << setfill('-') << "-" << endl;
		for(launchIter = launchGroups.begin(); launchIter != launchGroups.end(); launchIter++){
			//loop through processes
			for(Launchable launchable : launchIter->second){
				if(launchable.exitCode==0){
					cout << setw(2) << setfill(' ') << launchable.launchGroup << setw(5) << launchable.exitCode << setw(20) << launchable.path << setw(20) << launchable.params << endl;
				}
			}
		}
	}
	cout << endl;
	return 0;
}

//Controls operation of the app launcher
int main( int argc, char* argv[]){

	//Get text file directory from cmd line
	if(argc!=2){
		cout << "Error: invalid command line" << endl;
		return -1;
	}

	//store value
	istringstream iss( argv[1]);
	string filePath;
	iss >> filePath;

	//check for not null
	if( !iss ){
		cout << "Error: invalid directory" << endl;
		return -1;
	}

	//Get Launch Groups From File
	map<int, vector<Launchable>> launchGroups;
	getLaunchGroupsFromFile(filePath, launchGroups);

	//Launch Processes
	//iterate through map
	map<int, vector<Launchable>>::iterator launchIter;
	for(launchIter = launchGroups.begin(); launchIter != launchGroups.end(); launchIter++){
		//if async
		if(launchIter->second.size() > 1){
			launchGroupConcurrently(launchIter->second);
			//thread t(launchGroupConcurrently, std::ref(launchIter->second));
			//t.join();

		}
		//if sync
		else{
			launchGroupSerially(launchIter->second);
			cout << "1" << this_thread::get_id << endl;
		}
	}

	//print report
	printReport(launchGroups);

	return 0;
}





