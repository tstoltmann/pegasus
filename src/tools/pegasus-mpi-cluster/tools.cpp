#include <string>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifdef DARWIN
# include <sys/param.h>
# include <sys/sysctl.h>
#endif
#include <sys/time.h>
#include <sys/stat.h>
#include <limits.h>
#include <sstream>
#include <stdlib.h>
#include <libgen.h>
#ifdef LINUX
# include <sched.h>
# ifdef HAS_LIBNUMA
#  include <numaif.h>
# endif
#endif

#include "tools.h"
#include "failure.h"
#include "log.h"

using std::string;

/* purpose: formats ISO 8601 timestamp into given buffer (simplified)
 * paramtr: seconds (IN): time stamp
 *          buffer (OUT): where to put the results
 *          size (IN): capacity of buffer
 * returns: pointer to start of buffer for convenience. 
 */
char * isodate( time_t seconds, char* buffer, size_t size ) {
    struct tm zulu = *gmtime(&seconds);
    struct tm local = *localtime(&seconds);
    zulu.tm_isdst = local.tm_isdst;
    {
        time_t distance = (seconds - mktime(&zulu)) / 60;
        int hours = distance / 60;
        int minutes = distance < 0 ? -distance % 60 : distance % 60;
        size_t len = strftime( buffer, size, "%Y-%m-%dT%H:%M:%S", &local );
        snprintf( buffer+len, size-len, "%+03d:%02d", hours, minutes );
    }
    return buffer;
}


/* purpose: formats ISO 8601 timestamp into given buffer (simplified)
 * paramtr: seconds_wf (IN): time stamp with fractional seconds (millis)
 *          buffer (OUT): where to put the results
 *          size (IN): capacity of buffer
 * returns: pointer to start of buffer for convenience. 
 */
char * iso2date( double seconds_wf, char* buffer, size_t size ) {
    char millis[8]; 
    double integral, fractional = modf(seconds_wf,&integral); 
    time_t seconds = (time_t) integral; 
    struct tm zulu = *gmtime(&seconds);
    struct tm local = *localtime(&seconds);
    zulu.tm_isdst = local.tm_isdst;
    snprintf( millis, sizeof(millis), "%.3f", fractional ); 
    {
        time_t distance = (seconds - mktime(&zulu)) / 60;
        int hours = distance / 60;
        int minutes = distance < 0 ? -distance % 60 : distance % 60;
        size_t len = strftime( buffer, size, "%Y-%m-%dT%H:%M:%S", &local );
        snprintf( buffer+len, size-len, "%s%+03d:%02d", millis+1, hours, minutes );
    }
    return buffer;
}


/* Get the local host name */
void get_host_name(std::string &hostname) {
    char name[HOST_NAME_MAX];
    if (gethostname(name, HOST_NAME_MAX) < 0) {
        myfailures("Unable to get host name");
    }
    hostname = name;
}


/* Get the current time as the number of seconds since the epoch */
double current_time() {
    struct timeval now;
    gettimeofday(&now, NULL);
    double ts = now.tv_sec + (now.tv_usec/1000000.0);
    return ts;
}


/* Get the total amount of physical memory in bytes */
unsigned long get_host_memory() {
    unsigned long memory;
#ifdef DARWIN
    size_t size = sizeof(memory);
    if (sysctlbyname("hw.memsize", &memory, &size, NULL, 0) < 0) {
        myfailures("Unable to get host physical memory size");
    }
#else
    long pages = sysconf(_SC_PHYS_PAGES);
    if (pages < 0) {
        myfailures("Unable to get number of host physical memory pages");
    }
    long pagesize = sysconf(_SC_PAGE_SIZE);
    if (pagesize < 0) {
        myfailures("Unable to get physical page size");
    }
    memory = pages * pagesize;
#endif
    if (memory == 0) {
        myfailure("Invalid memory size: %lu bytes", memory);
    }
    return memory;
}


/* Get the total number of cpus on the host */
unsigned int get_host_cpus() {
    unsigned int cpus;
#ifdef __MACH__ 
    size_t size = sizeof(cpus);
    if (sysctlbyname("hw.physicalcpu", &cpus, &size, NULL, 0) < 0) {
        myfailures("Unable to get number of physical CPUs");
    }
#else
    long nprocessors = sysconf(_SC_NPROCESSORS_CONF);
    if (nprocessors <= 0) {
        myfailures("Unable to get number of physical CPUs");
    }
    cpus = nprocessors;
#endif
    if (cpus == 0) {
        myfailure("Invalid number of CPUs: %u", cpus);
    }
    return cpus;
}

int mkdirs(const char *path) {
    if (path == NULL || strlen(path) == 0) {
        return 0;
    }
    
    // No need to create cwd
    if (path[0] == '.' && path[1] == '\0') {
        return 0;
    }
    
    if (strlen(path) > PATH_MAX) {
        errno = ENAMETOOLONG;
        return -1;
    }
    
    char mypath[PATH_MAX];
    char *p = mypath;
    
    if (path[0] == '.' && path[1] == '.') {
        if (getcwd(mypath, PATH_MAX) == NULL) {
            return -1;
        }
        
        char *parent = strrchr(mypath, '/');
        if (parent) *parent = '\0';
        
        int off = strlen(mypath);
        strcpy(mypath+off, path+2);
        
        // In this case we don't need to go back to the root
        p = mypath + off; 
    } else if (path[0] == '.' && path[1] == '/') {
        strcpy(mypath, path+2);
    } else {
        strcpy(mypath, path);
    }
    
    while (*p == '/') p++;
    
    int created = 0;
    struct stat st;
    while ((p = strchr(p, '/'))) {
        *p = '\0';
        
        if (stat(mypath, &st)) {
            if (errno == ENOENT) {
                if (mkdir(mypath, 0777)) {
                    return -1;
                }
                created++;
            } else {
                return -1;
            }
        } else {
            // If it exists, make sure it is a dir
            if (S_ISDIR(st.st_mode) == 0) {
                errno = ENOTDIR;
                return -1;
            }
        }
        
        *p = '/';
        while (*p == '/') p++;
    }
    
    // Last element of path
    if (stat(mypath, &st)) {
        if (errno == ENOENT) { 
            if (mkdir(mypath, 0777)) {
                return -1;
            }
            created++;
        } else {
            return -1;
        }
    } else {
        if (S_ISDIR(st.st_mode) == 0) {
            errno = ENOTDIR;
            return -1;
        }
    }
    
    return created;
}

bool is_executable(const string &file) {
    // Invalid path
    if (file.size() == 0) {
        return false;
    }
    
    struct stat st;
    if (stat(file.c_str(), &st) == 0) {
        if (S_ISREG(st.st_mode)) {
            if ((st.st_uid == geteuid() && (S_IXUSR & st.st_mode) == S_IXUSR) ||
                (st.st_gid == getegid() && (S_IXGRP & st.st_mode) == S_IXGRP) ||
                ((S_IXOTH & st.st_mode) == S_IXOTH)) {
                // It is a regular file and we can execute it
                return true;
            }
        }
    }
    
    // In all other cases, the file is not executable
    return false;
}

int read_file(const string &file, char *buf, size_t size) {
    // Invalid path
    if (file.size() == 0) {
        errno = ENOENT;
        return -1;
    }
    
    FILE *f = fopen(file.c_str(), "r");
    if (f == NULL) {
        return -1;
    }
    
    size_t read = fread(buf, 1, size, f);

    if (fclose(f)) {
        return -1;
    }
    
    return read;
}

string pathfind(const string &file) {
    if (file.size() == 0) {
        return file;
    }
    
    // files that have a / should be returned as-is
    if (file.find('/') != string::npos) {
        return file;
    }
    
    // normally we wouldn't allow this
    if (is_executable(file)) {
        return file;
    }
    
    string path;
    char *env = getenv("PATH");
    if (env == NULL) {
#ifdef _PATH_DEFPATH
        path = _PATH_DEFPATH;
#else
        return file;
#endif
    } else {
        /* yes, there is a PATH variable */ 
        path = env;
    }
    
    string element;
    std::istringstream split(path);
    while(std::getline(split, element, ':')) {
        if (element.size() == 0) {
            continue;
        }
        string myfile = element + "/" + file;
        if (is_executable(myfile)) {
            return myfile;
        }
    }
    
    return file;
}

/* Return the directory part of a path */
string dirname(const string &path) {
    char *temp = strdup(path.c_str());
    string result = ::dirname(temp);
    free(temp);
    return result;
}

/* Return the last part of a path */
string filename(const string &path) {
    char *temp = strdup(path.c_str());
    string result = ::basename(temp);
    free(temp);
    return result;
}

int clear_cpu_affinity() {
#ifdef LINUX
    int cpus = get_host_cpus();
    cpu_set_t *cpuset = CPU_ALLOC(cpus);
    if (cpuset == NULL) {
        return -1;
    }
    size_t cpusetsize = CPU_ALLOC_SIZE(cpus);
    CPU_ZERO_S(cpusetsize, cpuset);

    int i;
    for (i=0; i<cpus; i++) {
        CPU_SET_S(i, cpusetsize, cpuset);
    }

    int rc = sched_setaffinity(0, cpusetsize, cpuset);
    CPU_FREE(cpuset);
    if (rc < 0) {
        return -1;
    }
#endif
    return 0;
}

int clear_memory_affinity() {
#ifdef HAS_LIBNUMA
    int rc = set_mempolicy(MPOL_DEFAULT, NULL, 0);
    if (rc < 0) {
        return -1;
    }
#endif
    return 0;
}
