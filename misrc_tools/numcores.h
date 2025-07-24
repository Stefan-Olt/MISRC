/*
* MISRC tools
* Copyright (C) 2024-2025  vrunk11, stefan_o
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


uint32_t get_num_cores() {
	uint32_t numberOfCores = 0;
#if defined(_WIN32) || defined(_WIN64)
	DWORD_PTR processAffinityMask;
	DWORD_PTR systemAffinityMask;
	if (GetProcessAffinityMask(GetCurrentProcess(), &processAffinityMask, &systemAffinityMask)) {
		while (processAffinityMask) {
			numberOfCores += processAffinityMask & 1;
			processAffinityMask >>= 1;
		}
		return numberOfCores;
	} else {
		// Handle error
		SYSTEM_INFO sysInfo;
		fprintf(stderr, "GetProcessAffinityMask failed with error to get number of cores: %lu, fallback to GetSystemInfo\n", GetLastError());
		GetSystemInfo(&sysInfo);
		return sysInfo.dwNumberOfProcessors;
	}
#elif defined(__linux__)
	cpu_set_t mask;
	if (sched_getaffinity(0, sizeof(cpu_set_t), &mask) == 0) {
		return CPU_COUNT(&mask);
	} else {
		// Handle error
		fprintf(stderr, "sched_getaffinity failed to get number of cores, fallback to sysconf\n");
		 // cores in deep-sleep are not counted with _SC_NPROCESSORS_ONLN, therefore using _CONF instead
		return sysconf(_SC_NPROCESSORS_CONF);
	}
#elif defined(__APPLE__) || defined(__MACH__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
    size_t size = sizeof(numberOfCores);
#if defined(__APPLE__) || defined(__MACH__)
	if (sysctlbyname("hw.logicalcpu", &numberOfCores, &size, NULL, 0) == 0) {
#else
	if (sysctlbyname("hw.ncpu", &numberOfCores, &size, NULL, 0) == 0) {
#endif
		return numberOfCores;
	} else {
		// Handle error
		fprintf(stderr, "failed to get number of cores\n");
		return 0;
	}
#else
	#warning "No code to get number of cores on this platform!"
	return 0;
#endif
}
