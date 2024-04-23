import pandas as pd
import matplotlib.pyplot as plt

data_path = "./fuzz_out/" + "time"
effi_path = "./fuzz_out/" + "effi"
output_name = "ble_8"

output_img_name = "./stats/" + output_name + ".png"
output_data_name = "./stats/" + output_name + ".csv"


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


eff_df = pd.read_csv(effi_path, names=['Time', 'Seed_Gen', 'Interesting', 'Crashes', 'Mut_Time', 'Driv_Time'])
stats = f'Total seed/runs: {eff_df["Seed_Gen"].sum()}\n'
stats += f'Avg time (ms): {((eff_df["Time"].sum()/eff_df["Seed_Gen"].sum())):.4f}\n'
stats += f'Total interesting: {eff_df["Interesting"].sum()}\n'
stats += f'Total crash: {eff_df["Crashes"].sum()}\n'
stats += f'Crash input ratio: {(eff_df["Crashes"].sum()/eff_df["Seed_Gen"].sum()):.4f}\n'
stats += f'Avg mutation time(ms): {((eff_df["Mut_Time"].sum()/eff_df["Seed_Gen"].sum())):.4f}\n'
stats += f'Avg driver time(ms): {((eff_df["Driv_Time"].sum()/eff_df["Seed_Gen"].sum())):.4f}'

plt.text(df['Time'].max() * 0.8, 0, stats, fontsize=10, bbox=dict(boxstyle='round,pad=0.5', facecolor='white', alpha=0.5))

plt.xlabel('Time')
plt.ylabel('Cumulative Occurrences')
plt.title('Cumulative Occurrences Over Time')
plt.legend()
plt.grid(True)

plt.xlabel('Time (seconds)')
plt.ylabel('Cumulative Paths Found')
plt.savefig(output_img_name)

df.to_csv(output_data_name, index=True)
