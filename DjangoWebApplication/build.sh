#!/usr/bin/env bash
# exit on error
set -o errexit

python3 -m pip install --upgrade pip

python3 -m pip install -r requirements.txt

python3 manage.py collectstatic --no-input
python3 manage.py makemigrations
python3 manage.py migrate

python3 manage.py generate-api -f
