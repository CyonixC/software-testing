import requests

# Replace 'http://<Django app URL>' with the base URL of your Django app (e.g., 'http://localhost:8000')
base_url = 'http://127.0.0.1:8000'

endpoint_url = '/'

url = base_url + endpoint_url

# Define the headers with cookies
headers = {
    'Cookie': 'csrftoken=jr6DahhKuGKgXX6Dxb3F4iR9FgiiQkAL; sessionid=bvlvh8bqcwhbzr2eqqk3blv9x5b68q4r'
}

try:
    # Send a GET request with the defined headers and cookies
    response = requests.get(url, headers=headers)

    # Check if the request was successful (status code 200)
    if response.status_code == 200:
        print("Request successful!")
        # Process the response data as needed
        print("Response:")
        print(response.text)
    else:
        print(f"Request failed with status code: {response.status_code}")
except requests.exceptions.RequestException as e:
    print("Request failed:", e)
