# Linux device driver learning path

A collection of example Linux kernel modules for learning device driver development.

author: Federico Roux [rouxfederico@gmail.com]

## Prerequisites

sudo apt-get install build-essential linux-headers-$(uname -r)## Building

# Build all examples
make

# Build specific example
cd 01-hello-world && make

# Or from root
make example ex=01-hello-world## Examples

1. **01-hello-world** - Basic module with init/exit

## Testing

cd 01-hello-world
make
make load    # Load module
make unload  # Unload module## Structure

Each example contains:
- `README.md` - Explanation and learning objectives
- `Makefile` - Build configuration
- `src/` - Source code
- `test/` - Userspace test programs (when applicable)
