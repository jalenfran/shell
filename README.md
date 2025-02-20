# JShell - Modern Unix Shell Implementation

A feature-rich Unix shell written in C that provides an interactive command-line interface with advanced features like command history, file completion, job control, and alias management.

## ✨ Features

### Core Functionality
- Command execution with argument handling
- Input/Output redirection (`>`, `>>`, `<`)
- Pipeline support (`|`)
- Background process execution (`&`)
- Environment variable management
- Job control (foreground/background processes)
- Alias management with name validation
- Command history with search
- Subshell support using ( ... ) for grouping commands
- Logical operators (`&&`, `||`)
- Support for control structures (`if`, `while`, `for`, `case`)

### Interactive Features
- Command history navigation (Up/Down arrows)
- Cursor movement (Left/Right arrows)
- Tab completion for commands and files
- Color-coded prompt and output
- Directory-aware path completion
- Intelligent command suggestions
- Error detection and reporting

### Scripting Features
- Shell script support with `.jsh` extension
- Environment variable expansion in scripts
- Command pipelines and redirection
- Background process management
- Error reporting with line numbers
- Comment support with `#`
- Full access to built-in commands
- Custom startup configuration via `.jshellrc`

### Built-in Commands

| Command             | Description                         |
|---------------------|-------------------------------------|
| `help`             | Display help message                |
| `cd [dir]`        | Change directory                    |
| `exit`            | Exit the shell                      |
| `env`             | Show environment variables          |
| `export VAR=value`| Set environment variable            |
| `unset VAR`       | Remove environment variable         |
| `alias name='cmd'`| Create or show aliases             |
| `unalias name`    | Remove an alias                     |
| `jobs`            | List background jobs                |
| `history`         | Lists all commands used            |
| `history [n]`     | Displays last `n` commands         |
| `fg [%job]`       | Bring job to foreground            |
| `bg [%job]`       | Continue job in background         |
| `kill %job`       | Terminate specified job            |

## 🌟 Advanced Features

### Alias Management
- Create and manage command aliases
- Protection against recursive aliases
- Strict name validation (letters, numbers, underscore)
- Support for aliases in pipelines
- Quote handling in alias values

### Job Control
- Background process management
- Job suspension (`Ctrl+Z`) and resumption
- Foreground/Background job switching
- Job status monitoring
- Process group management
- Signal handling (e.g., `SIGINT`, `SIGTSTP`)

### Input/Output Features
- Command history persistence
- Intelligent tab completion
- Directory-aware path completion
- Input line editing
- Signal handling (`Ctrl+C`, `Ctrl+Z`)
- Proper terminal control

## 🚀 Getting Started

### Prerequisites
- GCC compiler
- Make build system
- Unix-like operating system (Linux/macOS)

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

### Running the Shell

There are several ways to use JShell:

1. Direct command (after installation):
   ```bash
   jshell
   ```
2. Development version:
   ```bash
   ./bin/jshell
   ```
3. Running Scripts:
   ```bash
   # Method 1: Using jshell command
   jshell script.jsh
   
   # Method 2: Make script executable
   chmod +x script.jsh
   ./script.jsh
   ```

### Example Script (`example.jsh`)
```sh
# Print a welcome message
echo "Welcome to JShell!"

# List all files in the current directory
ls -la

# Create an alias for `ls`
alias ll='ls -l'

# Change directory to /tmp
cd /tmp

# Print environment variables
env
```

## 📁 Project Structure

```
shell/  
├── scripts/                # Directory for shell scripts
│   └── jshell-wrapper      # Wrapper script for JShell
├── src/                    # Source code files  
│   ├── alias.c             # Alias management implementation
│   ├── alias.h             # Alias management declarations
│   ├── builtin_commands.c  # Built-in commands implementation
│   ├── builtin_commands.h  # Built-in commands declarations
│   ├── builtin_commands_impl.c  # Implementation of built-in commands
│   ├── command.h           # Command structure and functions
│   ├── command_registry.c  # Command registry implementation
│   ├── command_registry.h  # Command registry declarations
│   ├── constants.h         # Shell constants and configurations
│   ├── executor.c          # Command execution logic
│   ├── history.c           # History management
│   ├── history.h           # History function declarations
│   ├── input.c             # Input handling and completion
│   ├── job_manager.c       # Job management implementation
│   ├── job_manager.h       # Job management declarations
│   ├── jobs_signals.c      # Signal handling for jobs
│   ├── main.c              # Shell initialization and main loop
│   ├── parser.c            # Command parsing and tokenization
│   ├── rc.c                # Configuration file handling
│   ├── rc.h                # Configuration file declarations
│   ├── shell.h             # Main shell header
├── bin/                    # Binary output directory  
│   └── jshell              # Compiled executable (generated)
├── obj/                    # Object files directory (generated)
│   ├── alias.o  
│   ├── builtin_commands.o  
│   ├── command_registry.o  
│   ├── executor.o  
│   ├── history.o  
│   ├── input.o  
│   ├── job_manager.o  
│   ├── jobs_signals.o  
│   ├── main.o  
│   ├── parser.o  
│   ├── rc.o  
├── .gitignore              # Git ignore file
├── .jshellrc               # JShell configuration file
├── LICENSE                 # License file (MIT)
├── Makefile                # Build configuration
```

## 📜 License
This project is licensed under the MIT License. See the `LICENSE` file for more details.

## 🤝 Contributing
Contributions are welcome! Feel free to submit issues, feature requests, or pull requests to improve JShell.

## 📞 Support
If you encounter any issues, check the GitHub Issues page or reach out to the maintainers.

Enjoy using JShell! 🚀