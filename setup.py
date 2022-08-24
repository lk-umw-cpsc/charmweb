from distutils.core import setup, Extension

def main():
    setup(name="chemu",
          version="1.0.0",
          description="Python interface for Chemu",
          author="Lauren Knight",
          author_email="lknight2@mail.umw.edu",
          ext_modules=[Extension("chemu", ["dummy.c"])])

if __name__ == "__main__":
    main()