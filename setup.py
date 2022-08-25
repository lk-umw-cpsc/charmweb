from distutils.core import setup, Extension

def main():
    setup(name="chemu",
          version="1.0.0",
          description="Python interface for Chemu",
          author="Lauren Knight",
          author_email="lknight2@mail.umw.edu",
          ext_modules=[Extension("chemu", ['chemupython.c', 'cpu.c', 'memory.c', 'command.c', 'isa.c', 'bit_functions.c', 'dict.c'],
          depends=['bit_functions.h', 'charmopcodes.h', 'cpu.h', 'dict.h', 'isa.h', 'memory.h'])])
#, "bit_functions.c", "command.c", "cpu.c", "dict.c", "isa.c", "memory.c"
if __name__ == "__main__":
    main()