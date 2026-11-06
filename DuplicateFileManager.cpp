#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>
#include <wincrypt.h>

#pragma comment(lib, "advapi32.lib")

typedef struct
{
    char FilePath[MAX_PATH];
    char Hash[33]; 
    int IsDeleted;
} FileInfo;

static int ComputeFileHash(const char* filePath, char* hashStr, size_t hashStrSize)
{
    FILE* f = fopen(filePath, "rb");
    if (!f) return 0;

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE rgbFile[16384];
    DWORD cbRead = 0;
    BYTE rgbHash[16];
    DWORD cbHash = 16;
    CHAR rgbDigits[] = "0123456789abcdef";
    int success = 0;

    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
        {
            while ((cbRead = (DWORD)fread(rgbFile, 1, sizeof(rgbFile), f)) > 0)
            {
                if (!CryptHashData(hHash, rgbFile, cbRead, 0))
                {
                    break;
                }
            }

            if (ferror(f) == 0)
            {
                if (CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0))
                {
                    if (hashStrSize >= 33)
                    {
                        for (DWORD i = 0; i < cbHash; i++)
                        {
                            hashStr[i * 2] = rgbDigits[rgbHash[i] >> 4];
                            hashStr[i * 2 + 1] = rgbDigits[rgbHash[i] & 0xf];
                        }
                        hashStr[cbHash * 2] = '\0';
                        success = 1;
                    }
                }
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
    fclose(f);
    return success;
}

static int LoadFolderFiles(const char* folderPath, FileInfo** outFiles)
{
    char searchPath[MAX_PATH] = { 0 };
    snprintf(searchPath, sizeof(searchPath), "%s\\*.*", folderPath);

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath, &findData);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        printf("Error: Could not open directory or directory is empty. Check your path.\n");
        return -1;
    }

    int capacity = 10;
    int count = 0;

    FileInfo* files = (FileInfo*)malloc(capacity * sizeof(FileInfo));
    if (!files)
    {
        printf("Memory allocation failed.\n");
        FindClose(hFind);
        return -1;
    }

    do
    {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            if (count >= capacity)
            {
                capacity *= 2;
                FileInfo* tempFiles = (FileInfo*)realloc(files, capacity * sizeof(FileInfo));
                if (!tempFiles)
                {
                    printf("Memory reallocation failed.\n");
                    break;
                }
                files = tempFiles;
            }

            snprintf(files[count].FilePath, MAX_PATH, "%s\\%s", folderPath, findData.cFileName);
            files[count].IsDeleted = 0;

            if (ComputeFileHash(files[count].FilePath, files[count].Hash, sizeof(files[count].Hash)))
            {
                count++;
            }
            else
            {
                printf("Warning: Could not read or hash file (skipping): %s\n", files[count].FilePath);
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
    *outFiles = files;
    return count;
}

static void HandleDuplicateChoice(FileInfo* file1, FileInfo* file2)
{
    char choice[10] = { 0 };

    printf("\n[DUPLICATE MATCH FOUND (Same MD5 Hash)]\n");
    printf("  [1] %s\n", file1->FilePath);
    printf("  [2] %s\n", file2->FilePath);
    printf("--------------------------------------------------------------\n");
    printf("Options:\n");
    printf("  [1] Delete File 1 (Keep File 2)\n");
    printf("  [2] Delete File 2 (Keep File 1)\n");
    printf("  [3] View File 1\n");
    printf("  [4] View File 2\n");
    printf("  [5] Skip / Keep both\n");
    printf("Enter your choice (1-5): ");

    if (fgets(choice, sizeof(choice), stdin) == NULL) return;
    choice[strcspn(choice, "\r\n")] = 0;

    char cmd = choice[0];

    switch (cmd)
    {
    case '1':
        if (remove(file1->FilePath) == 0)
        {
            printf("Successfully deleted File 1: %s\n", file1->FilePath);
            file1->IsDeleted = 1;
        }
        else
        {
            printf("Error: Could not delete File 1 (may be in use).\n");
        }
        break;

    case '2':
        if (remove(file2->FilePath) == 0)
        {
            printf("Successfully deleted File 2: %s\n", file2->FilePath);
            file2->IsDeleted = 1;
        }
        else
        {
            printf("Error: Could not delete File 2 (may be in use).\n");
        }
        break;

    case '3':
    {
        HINSTANCE res = ShellExecuteA(NULL, "open", file1->FilePath, NULL, NULL, SW_SHOWNORMAL);
        if ((intptr_t)res <= 32) printf("Error opening File 1.\n");
        HandleDuplicateChoice(file1, file2); 
        break;
    }

    case '4':
    {
        HINSTANCE res = ShellExecuteA(NULL, "open", file2->FilePath, NULL, NULL, SW_SHOWNORMAL);
        if ((intptr_t)res <= 32) printf("Error opening File 2.\n");
        HandleDuplicateChoice(file1, file2); 
        break;
    }

    case '5':
    default:
        printf("Skipped. Keeping both files.\n");
        break;
    }
}

int main()
{
    char folderPath[MAX_PATH] = { 0 };

    printf("--------------------------------------------------------------\n");
    printf("GENERAL FILE DEDUPLICATION ENGINE (MD5 HASHING)\n");
    printf("--------------------------------------------------------------\n");

    printf("Enter folder path to scan: ");

    if (fgets(folderPath, sizeof(folderPath), stdin) != NULL)
    {
        folderPath[strcspn(folderPath, "\r\n")] = 0;
    }
    else
    {
        printf("Error reading folder path.\n");
        return 1;
    }

    size_t len = strlen(folderPath);
    if (len > 0 && folderPath[0] == '"' && folderPath[len - 1] == '"')
    {
        folderPath[len - 1] = '\0';
        memmove(folderPath, folderPath + 1, len - 1);
    }

    FileInfo* files = NULL;
    int count = LoadFolderFiles(folderPath, &files);
    if (count < 0)
    {
        return 1;
    }

    printf("--------------------------------------------------------------\n");
    printf("Total number of files scanned and hashed: %d\n", count);
    printf("--------------------------------------------------------------\n");

    if (count < 2)
    {
        printf("Need at least 2 files in the folder to compare.\n");
        free(files);
        return 0;
    }

    for (int i = 0; i < count; i++)
    {
        if (files[i].IsDeleted) continue;

        for (int j = i + 1; j < count; j++)
        {
            if (files[j].IsDeleted) continue;

            if (strcmp(files[i].Hash, files[j].Hash) == 0)
            {
                HandleDuplicateChoice(&files[i], &files[j]);
                printf("--------------------------------------------------------------\n");

                if (files[i].IsDeleted) break;
            }
        }
    }
    free(files);
    return 0;
}