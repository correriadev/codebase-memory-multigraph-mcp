#include "horizon_reaper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__has_include)
  #if __has_include(<sqlite3.h>)
    #include <sqlite3.h>
  #elif __has_include("sqlite3.h")
    #include "sqlite3.h"
  #endif
#else
  #include <sqlite3.h>
#endif

#ifdef _WIN32
  #include <windows.h>
  #include <io.h>
  #define cbm_unlink(path) _unlink(path)

  bool cbm_is_pid_alive(uint32_t pid) {
      if (pid == 0) return false;
      HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
      if (h != NULL) {
          DWORD exit_code = 0;
          if (GetExitCodeProcess(h, &exit_code)) {
              CloseHandle(h);
              return (exit_code == STILL_ACTIVE);
          }
          CloseHandle(h);
          return true;
      }
      DWORD err = GetLastError();
      if (err == ERROR_ACCESS_DENIED) return true;
      return false;
  }
#else
  #include <dirent.h>
  #include <unistd.h>
  #include <signal.h>
  #include <errno.h>
  #include <sys/stat.h>
  #define cbm_unlink(path) unlink(path)

  bool cbm_is_pid_alive(uint32_t pid) {
      if (pid == 0) return false;
      if (kill((pid_t)pid, 0) == 0) return true;
      return (errno == EPERM);
  }
#endif

static void reap_single_horizon(const char *db_path, HorizonReaperStats *stats, uint64_t now) {
    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL);
    if (rc != SQLITE_OK) {
        if (db) sqlite3_close_v2(db);
        return;
    }

    uint32_t client_pid = 0;
    uint64_t created_at = 0;
    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(db, "SELECT client_pid, created_at FROM horizon_metadata LIMIT 1", -1, &stmt, NULL);
    if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
        client_pid = (uint32_t)sqlite3_column_int64(stmt, 0);
        created_at = (uint64_t)sqlite3_column_int64(stmt, 1);
    }
    sqlite3_finalize(stmt);
    sqlite3_close_v2(db);

    if (client_pid == 0) {
        return;
    }

    bool alive = cbm_is_pid_alive(client_pid);
    if (!alive && (now > created_at) && (now - created_at > CBM_HORIZON_TTL_SECONDS)) {
        /* Orphan and TTL exceeded -> unlink files */
        cbm_unlink(db_path);

        char wal[1024];
        snprintf(wal, sizeof(wal), "%s-wal", db_path);
        cbm_unlink(wal);

        char shm[1024];
        snprintf(shm, sizeof(shm), "%s-shm", db_path);
        cbm_unlink(shm);

        if (stats) stats->reaped_count++;
    } else {
        if (stats) stats->retained_count++;
    }
}

int cbm_reap_orphan_horizons(const char *horizons_dir, HorizonReaperStats *out_stats) {
    if (!horizons_dir) return -1;
    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }

    uint64_t now = (uint64_t)time(NULL);

#ifdef _WIN32
    char search_pattern[1024];
    snprintf(search_pattern, sizeof(search_pattern), "%s\\*.db", horizons_dir);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_pattern, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        return 0;
    }

    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s\\%s", horizons_dir, find_data.cFileName);
            reap_single_horizon(full_path, out_stats, now);
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
#else
    DIR *d = opendir(horizons_dir);
    if (!d) return 0;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len > 3 && strcmp(entry->d_name + len - 3, ".db") == 0) {
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", horizons_dir, entry->d_name);
            reap_single_horizon(full_path, out_stats, now);
        }
    }
    closedir(d);
#endif

    return 0;
}
