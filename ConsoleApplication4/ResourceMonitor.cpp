// ResourceMonitor.cpp : Defines the entry point for the console application.
//

#include <iostream>
#include <string>
#include <Pdh.h>
#include <PdhMsg.h>
#include <Windows.h>
#include "PdhHelperFunctions.h"
#include <conio.h>

#include <future>
#include <thread>


using namespace std;

void printStats(double diskUsage, unsigned long long downloadedBytes, unsigned long long uploadedBytes, 
			  PDH_FMT_COUNTERVALUE cpuUsage, wstring processTitle, double ramUsage) {
	wprintf(L"%5.2f  %u\t%u\t%5.2f   %s\t%.2f\n",
		diskUsage,
		(unsigned int)downloadedBytes,
		(unsigned int)uploadedBytes,
		cpuUsage.doubleValue,
		processTitle.c_str(),
		ramUsage);
}

void printColoredStats(double diskUsage, unsigned long long downloadedBytes, unsigned long long uploadedBytes,
	PDH_FMT_COUNTERVALUE cpuUsage, wstring processTitle, double ramUsage) {
	HANDLE  hConsole;
	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	if (diskUsage >= 75.0 || cpuUsage.doubleValue >= 75.0 || ramUsage >= 75.0) {
		FlushConsoleInputBuffer(hConsole);
		SetConsoleTextAttribute(hConsole, 12);

		printStats(diskUsage,
			downloadedBytes,
			uploadedBytes,
			cpuUsage,
			processTitle.c_str(),
			ramUsage);
		SetConsoleTextAttribute(hConsole, 15);
	}
	else if (diskUsage >= 50.0 || cpuUsage.doubleValue >= 50.0 || ramUsage >= 50.0) {
		FlushConsoleInputBuffer(hConsole);
		SetConsoleTextAttribute(hConsole, 14);

		printStats(diskUsage,
			downloadedBytes,
			uploadedBytes,
			cpuUsage,
			processTitle.c_str(),
			ramUsage);
		SetConsoleTextAttribute(hConsole, 15);
	}
	else
	{
		FlushConsoleInputBuffer(hConsole);
		SetConsoleTextAttribute(hConsole, 10);
		printStats(diskUsage,
			downloadedBytes,
			uploadedBytes,
			cpuUsage,
			processTitle.c_str(),
			ramUsage);
		SetConsoleTextAttribute(hConsole, 15);
	}
}

double GetPercentUsedRAM() {
	//Gets the system physical ram usage percent, returned as a double.
	MEMORYSTATUSEX data;
	data.dwLength = sizeof(data);
	if (GlobalMemoryStatusEx(&data) == 0) {
		cout << "GetPercentUsedRAM() GlobalMemoryStatusEx() failed." << endl;
		return 0.0;
	}
	double bytes_in_use = (double)(data.ullTotalPhys - data.ullAvailPhys);
	return bytes_in_use / ((double)data.ullTotalPhys) * 100;
}

double getDiskUsage(PDH_HCOUNTER disk_pct_counters, int sleep_time) {
	PDH_FMT_COUNTERVALUE_ITEM* disk_pcts = 0;
	DWORD counter_count = GetCounterArray(disk_pct_counters, PDH_FMT_DOUBLE, &disk_pcts);
	if (counter_count == 0) {
		sleep_time = 1;
	}
	double diskUsage = 0.0;
	for (DWORD diskN = 1; diskN < counter_count; ++diskN) {
		if (disk_pcts[diskN].FmtValue.doubleValue > diskUsage) {
			diskUsage = disk_pcts[diskN].FmtValue.doubleValue;
		}
	}
	delete[] disk_pcts;
	return diskUsage;
}

void startMegaCycle(boolean continueGettingStats, boolean colored) {
	PDH_HQUERY query_handle;
	PDH_STATUS pdh_status = PdhOpenQuery(NULL, 0, &query_handle);
	if (pdh_status != ERROR_SUCCESS) { //Error in init
		cout << "PdhOpenQuery() error." << endl;
	}

	PDH_HCOUNTER cpuUsage_counter = AddSingleCounter(query_handle,
		L"\\Processor(_Total)\\% Processor Time");
	PDH_HCOUNTER disk_pct_counters = AddSingleCounter(query_handle,
		L"\\PhysicalDisk(*)\\% Disk Time");
	PDH_HCOUNTER bytes_sent_counters = AddSingleCounter(query_handle,
		L"\\Network Interface(*)\\Bytes Sent/sec");
	PDH_HCOUNTER bytes_recv_counters = AddSingleCounter(query_handle,
		L"\\Network Interface(*)\\Bytes Received/sec");
	PDH_HCOUNTER process_cpuUsage_counters = AddSingleCounter(query_handle,
		L"\\Process(*)\\% Processor Time");
	PDH_HCOUNTER process_write_bytes_counters = AddSingleCounter(query_handle,
		L"\\Process(*)\\IO Write Bytes/sec");
	PDH_HCOUNTER process_read_bytes_counters = AddSingleCounter(query_handle,
		L"\\Process(*)\\IO Read Bytes/sec");

	//Collect first sample
	if (!CollectQueryData(query_handle)) {
		cout << "First sample collection failed." << endl;
	}

	int sleep_time = 1000;
	while (continueGettingStats) {
		
		if (GetAsyncKeyState(VK_RETURN)) {
			continueGettingStats = false;
		}
		Sleep(sleep_time);
		if (sleep_time != 1000) {
			sleep_time = 1000;
		}
		CollectQueryData(query_handle);

		////////// CPU % //////////
		PDH_FMT_COUNTERVALUE cpuUsage;
		pdh_status = PdhGetFormattedCounterValue(cpuUsage_counter, PDH_FMT_DOUBLE, 0, &cpuUsage);
		if ((pdh_status != ERROR_SUCCESS) || (cpuUsage.CStatus != ERROR_SUCCESS)) {
			sleep_time = 1;
			continue;
		}

		double diskUsage = getDiskUsage(disk_pct_counters, sleep_time);
		unsigned long long uploadedBytes = SumCounterArray(bytes_sent_counters);
		unsigned long long downloadedBytes = SumCounterArray(bytes_recv_counters);
		double ramUsage = GetPercentUsedRAM();

		wstring processTitle = L"";
		if (cpuUsage.doubleValue >= 90.0) {
			PDH_FMT_COUNTERVALUE_ITEM* process_cpuUsages = 0;
			DWORD process_count = GetCounterArray(process_cpuUsage_counters,
				PDH_FMT_DOUBLE,
				&process_cpuUsages);
			if (process_count == 0) {
			}
			else {
				double highest_cpuUsage = 0.0;
				DWORD index_of_highest_cpuUsage = 0;
				for (DWORD procN = 1; procN < process_count; ++procN) {
					if (process_cpuUsages[procN].FmtValue.doubleValue > highest_cpuUsage) {
						highest_cpuUsage = process_cpuUsages[procN].FmtValue.doubleValue;
						index_of_highest_cpuUsage = procN;
					}
				}
				if (index_of_highest_cpuUsage != 0) {
					processTitle.assign(process_cpuUsages[index_of_highest_cpuUsage].szName);
				}
				delete[] process_cpuUsages;
			}
		}
		else {
			PDH_FMT_COUNTERVALUE_ITEM* process_write_bytes = 0;
			DWORD process_count = GetCounterArray(process_write_bytes_counters,
				PDH_FMT_LARGE,
				&process_write_bytes);
			if (process_count == 0) {
			}
			else {
				PDH_FMT_COUNTERVALUE_ITEM* process_read_bytes = 0;
				process_count = GetCounterArray(process_read_bytes_counters,
					PDH_FMT_LARGE,
					&process_read_bytes);
				if (process_count == 0) {
					delete[] process_write_bytes;
				}
				else {
					if (diskUsage >= 50.0) {
						long long highest_total_io = 0;
						DWORD index_of_highest = 0;
						for (DWORD procN = 1; procN < process_count; ++procN) {
							long long total_io = process_write_bytes[procN].FmtValue.largeValue + process_read_bytes[procN].FmtValue.largeValue;
							if (total_io > highest_total_io) {
								highest_total_io = total_io;
								index_of_highest = procN;
							}
						}
						if (index_of_highest != 0) {
							//Save the process name for output
							processTitle.assign(process_write_bytes[index_of_highest].szName);
						}
					}
					else if (downloadedBytes > uploadedBytes) {
						//Find process with highest read IO
						long long highest_read_io = 0;
						DWORD index_of_highest = 0;
						for (DWORD procN = 1; procN < process_count; ++procN) {
							if (process_read_bytes[procN].FmtValue.largeValue > highest_read_io) {
								highest_read_io = process_read_bytes[procN].FmtValue.largeValue;
								index_of_highest = procN;
							}
						}
						if (index_of_highest != 0) {
							//Save the process name for output
							processTitle.assign(process_read_bytes[index_of_highest].szName);
						}
					}
					else if (uploadedBytes < downloadedBytes) {
						//Find process with highest write IO
						long long highest_write_io = 0;
						DWORD index_of_highest = 0;
						for (DWORD procN = 1; procN < process_count; ++procN) {
							if (process_write_bytes[procN].FmtValue.largeValue > highest_write_io) {
								highest_write_io = process_write_bytes[procN].FmtValue.largeValue;
								index_of_highest = procN;
							}
						}
						if (index_of_highest != 0) {
							//Save the process name for output
							processTitle.assign(process_write_bytes[index_of_highest].szName);
						}
					}
					else {
						//Nothing is happening
						//processTitle.assign(L"nothing");
					}
					delete[] process_write_bytes;
					delete[] process_read_bytes;
				}
			}
		}

		////////// Format Output //////////
		if (processTitle.length() < 25) {
			while (processTitle.length() < 25) {
				processTitle.push_back(' ');
			}
		}
		
		if (colored) {
			printColoredStats(diskUsage,
				downloadedBytes,
				uploadedBytes,
				cpuUsage,
				processTitle.c_str(),
				ramUsage);
		}
		else
		{
			printStats(diskUsage,
				downloadedBytes,
				uploadedBytes,
				cpuUsage,
				processTitle.c_str(),
				ramUsage);
		}
				
	}
}

void initTableHeader() {
	cout << "Resource Monitor\tDec 2016" << endl;
	cout << "Disk   Download\tUpload   CPU               Process  \t         RAM" << endl;
}

/* 
void listenKeyboardEvents(boolean continueGettingStats) {
	if (GetAsyncKeyState(VK_RETURN)) {
		continueGettingStats = false;
	}
	if (continueGettingStats) { 
		listenKeyboardEvents(continueGettingStats);
	}
}*/

void clearConsole() {
	system("cls");
}

void initWorking() {
	string str;
	cout << "To start monitoring enter \"start\"" << endl;
	cout << "To show modificators enter \"help\"" << endl;
	cout << "To exit from application enter \"exit\"" << endl;
	cin >> str;
	if (str.find("start") != string::npos) {
		boolean colored = false;
		if (str.find("-c") != string::npos) {
			colored = true;
		}
		GetAsyncKeyState(VK_RETURN);
		initTableHeader();
		boolean continueGettingStats = true;																														boolean continueGetingStats = true;
		thread loopTread(startMegaCycle, continueGettingStats, colored);
		loopTread.join();
		clearConsole();
		initWorking();
	}
	else if (str == "help") {
		cout << "\n\n\n Concat modifikators with command like as \" start-c\"." << endl;
		cout << "List of modificators:" << endl;
		cout << "-c -- print colored statistic\n\n\n" << endl;
		initWorking();
	}
	else if (str == "exit") {}
	else
	{
		cout << "Cant find this command" << endl;
		initWorking();
	}
}

void startWorking(boolean colored) {
	boolean continueGettingStats = true;																														boolean continueGetingStats = true;
	thread loopTread(startMegaCycle, continueGettingStats, colored);
	//thread keyListenTread(listenKeyboardEvents, continueGetingStats);
	//keyListenTread.join();
	loopTread.join();	
	clearConsole();
	initWorking();
}

int main()
{
	initWorking();
	return 0;
}
