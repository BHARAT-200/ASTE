# ASTE - A Small Text Editor

ASTE is a terminal-based text editor written in C with built-in file encryption using a custom RCEX (RC4-Extended) cipher. It provides essential editing features with syntax highlighting for C/C++ and includes integrated encryption/decryption for protecting sensitive files.

## Features

- Terminal-based editing with ANSI escape sequences
- C/C++ syntax highlighting (keywords, strings, numbers, comments)
- Line numbers
- Incremental search
- Bracket matching and auto-indentation for C/C++
- File encryption/decryption with custom RCEX cipher
- Message Authentication Code (MAC) for integrity verification
- Unsaved changes protection
- Basic text editing operations

## Keybindings

| Key            | Action                         |
|----------------|--------------------------------|
| `Ctrl-O`       | Open file                      |
| `Ctrl-S`       | Save file (with encryption option) |
| `Ctrl-Q`       | Quit                           |
| `Ctrl-F`       | Search                         |
| `Ctrl-G`       | Go to line                     |
| `Arrow Keys`   | Move cursor                    |
| `Home`         | Jump to start of line          |
| `End`          | Jump to end of line            |
| `Page Up/Down` | Scroll by page                 |
| `Backspace`    | Delete character before cursor |
| `Delete`       | Delete character at cursor     |
| `Enter`        | Insert newline (auto-indent)   |
| `/ASCI_HELP`   | Show help screen               |

## Installation

### Requirements

- C compiler (GCC or Clang)
- POSIX-compliant system (Linux, macOS, BSD)
- `make` utility

### Compilation

```bash
make
```

To clean build artifacts:

```bash
make clean
```

## Running ASTE

Start ASTE without a file (blank editor):

```bash
./ASTE
```

Open an existing file:

```bash
./ASTE filename.c
```

## Basic Usage

1. Launch ASTE with or without a filename
2. Edit text using standard editing keys
3. Save with `Ctrl-S`
4. Search with `Ctrl-F` (use arrow keys to navigate results)
5. Jump to a specific line with `Ctrl-G`
6. Exit with `Ctrl-Q`

## RCEX Encryption

ASTE includes integrated file encryption using RCEX, a custom stream cipher based on RC4. Encrypted files include authentication to detect tampering or incorrect keys.

### Encrypting a File

1. Open or create a file in ASTE
2. Press `Ctrl-S` to save
3. You'll see: `Save: [1] Normal  [2] RCEX Encrypt  ESC=Cancel`
4. Press `2` to encrypt
5. Enter the output filename
6. Enter your encryption key (input is masked with `*`)
7. The file is encrypted and saved

### File Format

RCEX-encrypted files use this structure:

```
Bytes 0-15:   Nonce (16 bytes, randomly generated)
Bytes 16-31:  MAC (16 bytes, for authentication)
Bytes 32+:    Ciphertext (same length as plaintext)
```

The MAC is computed over the ciphertext using a key derived from both the user password and the nonce. This ensures that:
- Wrong keys are rejected before decryption
- File tampering is detected
- Each encryption uses a unique keystream

### Decrypting a File

1. Press `Ctrl-O` to open a file
2. Enter the encrypted filename
3. You'll see: `Decrypt with RCEX? (y/n):`
4. Press `y` to decrypt (or `n` for normal plaintext)
5. Enter your encryption key (input is masked)
6. If the key is correct, the plaintext loads into the editor
7. If the key is wrong: `RCEX open failed: wrong key or file is corrupted/tampered`

### Key Input

Encryption keys:
- Can be any length (at least 1 character)
- Are never displayed on screen (shown as `*`)
- Are wiped from memory after use
- Must match exactly between encryption and decryption

## Project Structure

```
ASTE/
├── ASTE.c           # Main editor implementation
├── ASTE.h           # Header file with structures and function declarations
├── Makefile         # Build configuration
├── RCEX_enc/
│   ├── rcex.c       # RCEX cipher implementation
│   └── rcex.h       # RCEX header with types and function exports
└── README.md        # This file
```

### Key Components

- **Terminal I/O**: Raw mode terminal handling with `termios`, ANSI escape sequences for rendering
- **Editor State**: `struct editorConfig` holds cursor position, file content, screen dimensions, status messages
- **Row Management**: Dynamic array of `erow` structures representing text lines
- **Syntax Highlighting**: Token-based highlighting for C/C++ with multi-line comment support
- **File I/O**: Standard file operations with integrated RCEX encryption/decryption
- **RCEX Integration**: Stream cipher encryption with nonce-based key derivation and MAC authentication

## Syntax Highlighting

Currently supports C/C++ files (`.c`, `.h`, `.cpp`):

- Keywords: `if`, `while`, `for`, `return`, `struct`, etc.
- Types: `int`, `char`, `void`, `float`, etc.
- Strings (with escape sequences)
- Numbers
- Single-line comments (`//`)
- Multi-line comments (`/* */`)

Bracket matching highlights matching pairs of `()`, `[]`, `{}` when the cursor is near them.

## Technical Details

- **Language**: C (C23 standard)
- **Terminal**: POSIX `termios`, ANSI/VT100 escape sequences
- **Memory**: Dynamic allocation with careful cleanup
- **Encryption**: Custom RCEX stream cipher with MAC authentication
- **Platform**: POSIX-compliant systems (tested on macOS/Linux)

### RCEX Implementation

RCEX is a custom stream cipher based on RC4's key scheduling algorithm (KSA) with modifications:
- Nonce-based key derivation for unique keystreams
- Whitewashing (discarding first 5000 bytes) to reduce keystream bias
- MAC authentication to detect tampering and wrong keys
- Constant-time MAC comparison to prevent timing attacks

The cipher generates a keystream by XORing plaintext with pseudorandom bytes.

## Limitations

- **Syntax highlighting**: Only C/C++ currently supported
- **Terminal-only**: Requires ANSI/VT100 compatible terminal
- **Single file**: No tabs or multiple file buffers
- **Memory**: Entire file loaded into memory
- **Encryption**: RCEX is a custom cipher not audited by cryptographers
- **Line endings**: Handles `\n` and `\r\n`, but always saves with `\n`
- **Unicode**: Basic ASCII support, limited multi-byte character handling

## Security Notes

**RCEX is a custom stream cipher implementation** and has not undergone professional cryptographic review or formal security analysis. While it includes authentication (MAC) to detect tampering and wrong keys, it should not be considered equivalent to well-established ciphers like AES-GCM.

Key security features:
- Keys are masked during input
- Keys and plaintext are wiped from memory after use
- MAC prevents decryption with wrong keys
- Nonces ensure unique keystreams per encryption

Key limitations:
- Custom cipher without formal security proof
- No key derivation function (raw password used directly)
- No protection against side-channel attacks
- Keys vulnerable to keyloggers, memory dumps, etc.

**For sensitive data**, consider using established encryption tools like GPG, age, or OpenSSL instead.

## Building from Source

```bash
git clone <repository-url>
cd ASTE
make
./ASTE
```

## Contributing

Contributions are welcome. Please ensure:
- Code follows the existing style
- Changes compile without warnings (`-Wall -Wextra -Werror`)
- Basic functionality is tested manually

## Acknowledgments

ASTE is inspired by the [kilo](https://github.com/antirez/kilo) text editor and builds upon its terminal handling approach.

## Author

Developed as a learning project for systems programming and applied cryptography.
