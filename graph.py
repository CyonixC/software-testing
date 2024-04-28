import pandas as pd
import matplotlib.pyplot as plt

file_paths = ['./stats/coapp_1.csv', './stats/coapp_2.csv', './stats/coapp_3.csv','./stats/coapp_4.csv']
name_path =['Fuzzing with BASE = 1, exponential energy','Fuzzing with no interesting','Fuzzing with BASE = 5, exponential energy','Fuzzing with constant energy 100']
plt.figure(figsize=(10, 6))

line_styles = ['-', '--', '-.', ':']
colors = ['b', 'g', 'r', 'c', 'm', 'y', 'k']

for i, file_path in enumerate(file_paths):
    df = pd.read_csv(file_path)
    
    plt.plot(df['Time'], df['Cumulative_C'], label=name_path[i],
             linestyle=line_styles[i % len(line_styles)], color=colors[i % len(colors)])

# Configure plot settings
plt.xlabel('Time (seconds)')
plt.ylabel('Cumulative Crashes')
plt.title('Cumulative Crash Occurrences Over Time')
plt.legend()
plt.grid(True)

plt.savefig('./stats/cumulative_crashes.png')


