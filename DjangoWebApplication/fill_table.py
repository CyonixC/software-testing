import requests
import random
import json

base_url = 'http://127.0.0.1:8000/datatb/product/'

endpoint_url = 'add/'

url = base_url + endpoint_url

random_name = ''.join(random.choices('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ', k=10))
random_info = ''.join(random.choices('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ', k=10))
random_price = str(random.randint(1, 100))

form_data = {
    'name': random_name,
    'info': random_info,
    'price': random_price
}

# Define the headers with cookies
headers = {
    'Cookie': 'csrftoken=5vvs6151ScRQGpdMlKAf8FAFERO67MmK; sessionid=c35o5m7xkymbjdtcu9k916f8jfj2f8x7', # Optional
}

try:
    print(json.dumps(form_data))
    response = requests.post(url, headers=headers, data=json.dumps(form_data))

    # Check if the request was successful (status code 200)
    if response.status_code == 200:
        print("Request successful!")
        print("Response:")
        print(response.text)

    else:
        print(f"Request failed with status code: {response.status_code}")
except requests.exceptions.RequestException as e:
    print("Request failed:", e)
