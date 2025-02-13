# JShell - Modern Unix Shell Implementation

A feature-rich Unix shell written in C that provides an interactive command-line interface with advanced features like command history, file completion, and job control.

## ✨ Features

### Core Functionality
- Command execution with argument handling
- Input/Output redirection (`>`, `>>`, `<`)
- Pipeline support (`|`)
- Background process execution (`&`)
- Environment variable management
- Job control (foreground/background processes)

### Interactive Features
- Command history navigation (Up/Down arrows)
- Cursor movement (Left/Right arrows)
- Tab completion for commands and files
- Color-coded prompt and output
- Intelligent command suggestions

### Built-in Commands
| Command | Description |
|---------|-------------|
| `help` | Display help message |
| `cd [dir]` | Change directory |
| `exit` | Exit the shell |
| `env` | Show environment variables |
| `export VAR=value` | Set environment variable |
| `unset VAR` | Remove environment variable |
| `fg [%job]` | Bring job to foreground |
| `bg [%job]` | Continue job in background |
| `kill %job` | Terminate specified job |

## 🚀 Getting Started

### Prerequisites
- GCC compiler
- Make build system
- Unix-like operating system

### Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/jalenfran/shell.git
   cd shell
   ```

2. Build the project:
   ```bash
   make
   ```

3. Run the shell:
   ```bash
   ./bin/jshell
   ```

## 📁 Project Structure
shell/ 
├── src/ # Source code files 
│ ├── main.c # Shell initialization and main loop 
│ ├── executor.c # Command execution logic │ 
├── parser.c # Command parsing and tokenization 
│ ├── input.c # Input handling and completion │ 
├── history.c # History management 
│ ├── shell.h # Main header declarations 
│ ├── history.h # History function declarations 
│ └── constants.h # Shell constants and configurations 
├── bin/ # Binary output directory 
│ └── jshell # Compiled executable (generated) 
├── obj/ # Object files directory (generated) 
│ ├── main.o 
│ ├── executor.o 
│ ├── parser.o 
│ ├── input.o 
│ └── history.o 
├── Makefile # Build configuration 