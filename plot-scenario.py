import pandas as pd
from matplotlib import pyplot as plt
import scienceplots
import matplotlib as mpl

plt.style.use(['ieee', 'science'])
mpl.rcParams['legend.frameon'] = 'True'

positions_df = pd.read_csv('scenario-topology.txt',
                           sep=' ', names=['dev', 'cell_id', 'x', 'y'])
links_df = pd.read_csv(
    'IAB-topology.txt', sep=' ', lineterminator='\n', skip_blank_lines=True)

# Draw IAB donor and IAB nodes
fig = plt.figure(figsize=[5, 3], dpi=300)
plt.grid()

# Draw parent <--> child links
for idx, row in links_df.iterrows():
    parent_cid = row['parent_cell_id']
    child_cid = row['cell_id']

    child_row = positions_df.loc[positions_df['cell_id']
                                 == child_cid].reset_index()
    parent_row = positions_df.loc[positions_df['cell_id']
                                  == parent_cid].reset_index()

    # Draw line
    x_pos = [child_row.iloc[0]['x'], parent_row.iloc[0]['x']]
    y_pos = [child_row.iloc[0]['y'], parent_row.iloc[0]['y']]
    plt.plot(x_pos, y_pos, color='black', linewidth=0.8)

    # Draw arrow
    if x_pos[1] != x_pos[0]:
        def f_x(x): return x * (y_pos[1] - y_pos[0])/(x_pos[1] - x_pos[0]) + \
            y_pos[0] - x_pos[0] * (y_pos[1] - y_pos[0])/(x_pos[1] - x_pos[0])
        x_m = (x_pos[0] + x_pos[1])/2
        x_mplus = x_pos[0] + (x_pos[1] - x_pos[0]) * 0.6
        plt.arrow(x_m, f_x(x_m), x_mplus - x_m, f_x(x_mplus) - f_x(x_m), shape='full',
                  lw=0, length_includes_head=True, head_width=4, color='black')
    else:
        # Vertical line
        y_m = (y_pos[0] + y_pos[1])/2
        y_mplus = y_pos[0] + (y_pos[1] - y_pos[0]) * 0.6
        plt.arrow(x_pos[0], y_m, 0, y_mplus - y_m, shape='full',
                  lw=0, length_includes_head=True, head_width=4, color='black')


# Dummy plot for legend
plt.plot([0, 0], [0, 0], color='black',
         linestyle=None, label='Active MT-DU link')

iab_nodes_df = positions_df[positions_df['dev'] == 'iabnode']
plt.plot(iab_nodes_df['x'], iab_nodes_df['y'],
         marker='o', linestyle="None", label='IAB node')
donor_df = positions_df[positions_df['dev'] == 'donor']
plt.plot(donor_df['x'], donor_df['y'], marker='s',
         linestyle="None", label='IAB donor')
plt.legend()
plt.xlabel('X [m]')
plt.ylabel('Y [m]')
plt.savefig('iab-scenario.png')
