import pandas as pd
import matplotlib.pyplot as plt

data_path = "./fuzz_out/" + "time"
output_path = "./stats/" + "some_test.png"

# Parse data into a DataFrame
df = pd.read_csv(data_path, names=['Type', 'Time'])
df['Time'] = pd.to_numeric(df['Time'])

# Calculate cumulative counts
df['Time'] /= 1000
df['Cumulative_I'] = (df['Type'] == 'I').cumsum()
df['Cumulative_C'] = (df['Type'] == 'C').cumsum()
df['Cumulative_All'] = df['Cumulative_I'] + df['Cumulative_C']

# Plotting
plt.figure(figsize=(10, 6))
plt.plot(df['Time'], df['Cumulative_I'], label='Interesting')
plt.plot(df['Time'], df['Cumulative_C'], label='Crash')
plt.plot(df['Time'], df['Cumulative_All'], label='All')

plt.xlabel('Time')
plt.ylabel('Cumulative Occurrences')
plt.title('Cumulative Occurrences Over Time')
plt.legend()
plt.grid(True)

plt.xlabel('Time (seconds)')
plt.ylabel('Cumulative Paths Found')
plt.savefig(output_path)
