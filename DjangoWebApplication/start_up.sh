#!/bin/bash

# Check if virtualenv is installed
if ! command -v virtualenv &> /dev/null
then
    echo "virtualenv could not be found. Please install it first."
    exit
fi

# Create a virtual environment named 'env'
virtualenv env

# Activate the virtual environment
# This command might differ based on the shell you're using; this is for bash.
source env/bin/activate

# Install dependencies from requirements.txt
pip3 install -r requirements.txt

# Start the Django project
python manage.py runserver

# When you're done, you can deactivate the virtual environment by typing `deactivate`
