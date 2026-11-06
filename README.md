# Duplicate-File-Detector

A C-based Windows application that identifies duplicate files in a folder using MD5 hashing and allows users to manage duplicates through an interactive command-line interface.

## Features
- Scans files in a specified folder.
- Generates MD5 hashes for each file.
- Identifies duplicate files by comparing hashes.
- Allows users to view, delete, or skip duplicate files.

## Technologies Used
- C
- Windows API
- Windows Cryptography API
- MD5 Hashing

## Note
- Scans files in the specified folder only (no subdirectories).
- Deleted files cannot be recovered through the application.
