# Bug checking
To feed the bug files to the applications, the bug checker codes need to be compiled, and then the appropriate `.json` file containing the bug input needs to be fed to the compiled program.

## Django
*Requirements: Python 3 and Django environment*

### Setting up Django environment
Our project uses [pipenv](https://pipenv.pypa.io/en/latest/installation.html) for environment management for Python.

```shell
cd DjangoWebApplication
pipenv install
pipenv shell
```

### Running Django checker
```shell
# (in root folder)
make django_bug_checker
./bin/bug_checker.out <path_to_json_file>
```

## CoAP
*Requirements: Python 2*

### Setting up CoAP environment
***IMPORTANT***:
The Python path needs to be **manually** changed in the code to direct the program to the right version of python 2 **before** attempting to compile the code.

The python 2 path must be modified at line `199` in `CoAPthon/coap_bug_checking.cpp`. Change this to the location of your local install of Python2.

### Running CoAP checker
```shell
# (in root folder)
make coap_bug_checker
./bin/bug_checker.out <path_to_json_file>
```

# Fuzzing
If attempting to run the fuzzers, a few more environment setup steps are needed.

## Django
The environment setup is identical to the Django instructions above. Please refer to the instructions in `Setting up Django Environment` above.

Additionally, the [SQLite](https://www.sqlite.org/download.html) C++ library must be installed to collect coverage information. The code should work after installation.

## CoAP
***IMPORTANT***: The python path must be manually changed in the code file. Please refer to `Setting up CoAP environment` above.

In addition, [SQLite](https://www.sqlite.org/download.html) must be installed to collect coverage information, and the `coverage` module from PyPI must be installed as well.

