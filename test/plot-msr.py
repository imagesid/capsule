import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from io import StringIO
import matplotlib.cm as cm
from matplotlib.patches import Patch

# === Input data ===
df = pd.read_csv("logs/result-msr.csv")

df["Filename"] = df["Filename"].str.replace(
    r"\.oracleGeneral$", "", regex=True
)


df.set_index("Filename", inplace=True)
# Convert miss ratio → hit ratio (%)
df = (1.0 - df) * 100

df.columns = df.columns.str.replace("_", "-", regex=False)
# df.index   = df.index.str.replace("_", "-", regex=False)


# === Colormap and hatch styles ===
num_policies = len(df.columns)
viridis = cm.get_cmap('viridis', num_policies)
colors = [viridis(i) for i in range(num_policies)]

# Hatch patterns applied to every other bar
hatch_patterns = ['/', '\\', '|', '-', '+']
hatches = [hatch_patterns[i % len(hatch_patterns)] if i % 2 == 1 else None for i in range(num_policies)]

# === Plot setup ===
fig, ax = plt.subplots(figsize=(8, 2.2))
bar_width = 0.075
x = np.arange(len(df.index))

# === Plot bars with optional hatches ===
for idx, (col, color) in enumerate(zip(df.columns, colors)):
    offset = (idx - num_policies / 2) * bar_width
    ax.bar(x + offset, df[col], bar_width, color=color, edgecolor='white', linewidth=0.8,
           hatch=hatches[idx] if hatches[idx] else None)

# === Add value labels ===
for i in range(len(df.index)):
    for j, col in enumerate(df.columns):
        val = df.iloc[i, j]
        offset = (j - num_policies / 2) * bar_width
        # ax.text(i + offset, val - 1, f'{val:.1f}', ha='center', va='top', fontsize=6, color='white')

# === Custom legend with hatch and color ===
legend_elements = [
    Patch(facecolor=colors[i], edgecolor='white',
          hatch=hatches[i]* 4 if hatches[i] else '', label=col)
    for i, col in enumerate(df.columns)
]
ax.legend(handles=legend_elements, fontsize=5, ncol=6, loc='upper center',
          bbox_to_anchor=(0.5, -0.22), frameon=False)

# === Style and axis labels ===
ax.set_xlabel("Workload", fontsize=6)
ax.set_ylabel("Hit Ratio (%)", fontsize=6)
ax.set_xticks(x)
ax.set_xticklabels(df.index, rotation=0, ha='center', fontsize=7)
ax.tick_params(axis='both', labelsize=6)
ax.grid(True, linestyle='--', linewidth=0.3, axis='y')

plt.tight_layout()

# === Save ===
filename = "logs/hit-msr.png"
plt.savefig(filename, dpi=300, bbox_inches="tight")
plt.show()
print(f"Figure saved as: {filename}")
