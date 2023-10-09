#from os import stat
import numpy as np
import pandas as pd

NUM_DATA_SYM_PER_SLOT = int(14 - 2)
HEADER_SIZE = 20


def check_errors(campaign, result):
    result_filenames = campaign.db.get_result_files(result['meta']['id'])
    result_filename = result_filenames['stderr']
    with open(result_filename, 'r') as result_file:
        error_file = result_file.read()
        return len(error_file)


def get_udp_port_to_iab_depth_mapping(campaign, result, topology_tracename, first_udp_port, donor_cid):
    topology_filename = campaign.db.get_result_files(
        result['meta']['id'])[topology_tracename]
    top_df = pd.read_csv(
        topology_filename, sep=' ', lineterminator='\n', skip_blank_lines=True)

    port_depth_dict = {}
    for c_id in top_df['cell_id']:
        node_df = top_df[top_df['cell_id'] == c_id].iloc[0]
        depth = 1
        while node_df['parent_cell_id'] != donor_cid:
            parent_cid = node_df['parent_cell_id']
            node_df = top_df[top_df['cell_id'] == parent_cid].iloc[0]
            depth = depth + 1
        port_depth_dict[c_id + first_udp_port - 1] = depth

    return port_depth_dict

def get_cell_id_to_iab_depth_mapping(campaign, result, topology_tracename, donor_cid):
    topology_filename = campaign.db.get_result_files(
        result['meta']['id'])[topology_tracename]
    top_df = pd.read_csv(
        topology_filename, sep=' ', lineterminator='\n', skip_blank_lines=True)

    cell_id_depth_dict = {}
    for c_id in top_df['cell_id']:
        node_df = top_df[top_df['cell_id'] == c_id].iloc[0]
        depth = 1
        while node_df['parent_cell_id'] != donor_cid:
            parent_cid = node_df['parent_cell_id']
            node_df = top_df[top_df['cell_id'] == parent_cid].iloc[0]
            depth = depth + 1
        cell_id_depth_dict[c_id] = depth

    cell_id_depth_dict[donor_cid] = 0
    return cell_id_depth_dict


def get_e2e_avg_system_throughput(campaign, results, tracename, app_run_time):
    thr_avg = []
    src_rate_avg = []
    empty = 0
    for result in results:
        if (check_errors(campaign, result) != 0):
            continue

        result_filename = campaign.db.get_result_files(
            result['meta']['id'])[tracename]
        app_df = pd.read_csv(
            result_filename, sep=' ', lineterminator='\n', skip_blank_lines=True)

        if (app_df.size == 0):
            thr_avg.append(0)
            empty = empty + 1
            continue

        app_df = app_df.astype({'packet_size': 'int'})
        app_df['packet_size'] = app_df['packet_size'] + HEADER_SIZE
        app_df = app_df.astype({'port_to': 'int'})
        rx_df = app_df[app_df['rx_tx'] == 'Rx']
        tx_df = app_df[app_df['rx_tx'] == 'Tx']
        thr_entry = rx_df['packet_size'].sum(
        ) / (app_run_time) * 8 / 1e6  # [Bytes -> Mbit/s]
        src_rate = tx_df['packet_size'].sum(
        ) / (app_run_time) * 8 / 1e6  # [Bytes -> Mbit/s]
        thr_avg.append(thr_entry)
        src_rate_avg.append(src_rate)

    if (empty > 0):
        print(f"Warning: {empty} empty traces")

    return thr_avg, src_rate_avg

def get_e2e_avg_ue_system_throughput(campaign, results, tracename, app_run_time, portRx):
    thr_avg = []
    src_rate_avg = []
    
    empty = 0
    for result in results:
        if (check_errors(campaign, result) != 0):
            continue

        result_filename = campaign.db.get_result_files(
            result['meta']['id'])[tracename]
        app_df = pd.read_csv(
            result_filename, sep=' ', lineterminator='\n', skip_blank_lines=True)

        if (app_df.size == 0):
            thr_avg.append(0)
            empty = empty + 1
            continue

        app_df = app_df.astype({'packet_size': 'int'})
        app_df['packet_size'] = app_df['packet_size'] + HEADER_SIZE
        app_df = app_df.astype({'port_to': 'int'})
        app_df = app_df[app_df['port_to'] == portRx]

        rx_df = app_df[app_df['rx_tx'] == 'Rx']
        tx_df = app_df[app_df['rx_tx'] == 'Tx']
        thr_entry = rx_df['packet_size'].sum(
        ) / (app_run_time) * 8 / 1e6  # [Bytes -> Mbit/s]
        src_rate = tx_df['packet_size'].sum(
        ) / (app_run_time) * 8 / 1e6  # [Bytes -> Mbit/s]
        thr_avg.append(thr_entry)
        src_rate_avg.append(src_rate)

    if (empty > 0):
        print(f"Warning: {empty} empty traces")

    return thr_avg, src_rate_avg


def get_e2e_avg_system_throughput_vs_depth(campaign, results, app_tracename, topology_tracename, app_run_time, first_udp_port, donor_cid):
    thr_avg = []
    empty = 0

    out_df = pd.DataFrame(columns=['depth', 'metric'])

    for result in results:

        run_df = pd.DataFrame(columns=['depth', 'metric'])
        if (check_errors(campaign, result) != 0):
            continue

        port_depth_dict = get_udp_port_to_iab_depth_mapping(
            campaign, result, topology_tracename, first_udp_port, donor_cid)

        result_filename = campaign.db.get_result_files(
            result['meta']['id'])[app_tracename]
        app_df = pd.read_csv(
            result_filename, sep=' ', lineterminator='\n', skip_blank_lines=True)

        if (app_df.size == 0):
            thr_avg.append(0)
            empty = empty + 1
            continue

        app_df = app_df.astype({'packet_size': 'int'})
        app_df = app_df.astype({'port_to': 'int'})
        rx_df = app_df[app_df['rx_tx'] == 'Rx']

        for port in set(rx_df['port_to']):

            temp_df = rx_df[rx_df['port_to'] == port]

            thr_entry = temp_df['packet_size'].sum(
            ) / (app_run_time) * 8 / 1e6  # [Bytes -> Mbit/s]

            df_entry = pd.DataFrame([[port_depth_dict[port], thr_entry]],
                                    columns=['depth', 'metric'])
            run_df = pd.concat([run_df, df_entry])

        out_df = pd.concat(
            [out_df, run_df.groupby('depth').mean().reset_index()])

    if (empty > 0):
        print(f"Warning: {empty} empty traces")

    return out_df.groupby('depth').mean().reset_index()


def get_e2e_avg_latency_vs_depth(campaign, results, app_tracename, topology_tracename, app_run_time, first_udp_port, donor_cid):
    lat_avg = []
    empty = 0

    out_df = pd.DataFrame(columns=['depth', 'metric'])

    for result in results:
        run_df = pd.DataFrame(columns=['depth', 'metric'])
        if (check_errors(campaign, result) != 0):
            continue

        port_depth_dict = get_udp_port_to_iab_depth_mapping(
            campaign, result, topology_tracename, first_udp_port, donor_cid)

        result_filename = campaign.db.get_result_files(
            result['meta']['id'])[app_tracename]
        app_df = pd.read_csv(result_filename, sep=' ',
                             lineterminator='\n', skip_blank_lines=True)

        if (app_df.size == 0):
            continue

        rx_df = app_df[app_df['rx_tx'] == 'Rx']
        rx_df = rx_df.astype({'port_to': 'int'})
        rx_df = rx_df.astype({'delay': 'int'})

        for port in set(rx_df['port_to']):
            temp_df = rx_df[rx_df['port_to'] == port]

            lat_entry = temp_df['delay'].mean() / 1e6  # [ns -> ms]

            df_entry = pd.DataFrame([[port_depth_dict[port], lat_entry]],
                                    columns=['depth', 'metric'])
            run_df = pd.concat([run_df, df_entry])

        out_df = pd.concat(
            [out_df, run_df.groupby('depth').mean().reset_index()])

    return out_df.groupby('depth').mean().reset_index()



def get_rlc_or_pdcp_throughput_samples(cam, results, rlc_or_pdcp_tracename, phy_tracename, app_run_time):
    thr = {
        'tx': {},
        'rx': {}
    }
    numUes = {}

    # get number of UEs from PHY traces
    first_res = next(iter(results))
    temp_fn = cam.db.get_result_files(first_res['meta']['id'])[phy_tracename]
    temp_df = pd.read_csv(temp_fn, sep='\t', header=0)
    temp_df = temp_df.astype(
        {'DL/UL': 'string', 'time': 'float', 'tbSize': 'int32', 'corrupt': 'int32'})
    for gnb in set(temp_df['cellId']):
        thr['tx'][gnb] = []
        thr['rx'][gnb] = []
        temp_gnb_df = temp_df[temp_df['cellId'] == gnb]
        numUes[gnb] = len(set(temp_gnb_df['rnti']))
    for result in results:
        if (check_errors(cam, result) != 0):
            continue
        result_filenames = cam.db.get_result_files(result['meta']['id'])
        result_filename = result_filenames[rlc_or_pdcp_tracename]
        rx_df = pd.read_csv(result_filename, delimiter="\t", skiprows=1, header=None,
                            names=['start [s]', 'end [s]', 'Cell ID', 'IMSI', 'RNTI',
                                   'LCID', 'num tx PDUs', 'txBytes', 'num rx PDUs',
                                   'rxBytes', 'delay [s]', 'delay std dev', 'delay min',
                                   'delay max', 'PduSize', 'PduSize std dev', 'size min', 'size max'],
                            skip_blank_lines=True, index_col=False)
        if (rx_df.size == 0):
            assert (0 == 1)

        # group by UE and gNB pairs
        for gnb in set(rx_df['Cell ID']):
            rx_df_gnb = rx_df[rx_df['Cell ID'] == gnb]
            # add ues which are not present in the RLC trace
            numRlcUes = len(set(rx_df_gnb['RNTI']))
            if (numRlcUes < numUes[gnb]):
                for idx in range(numUes[gnb] - numRlcUes):
                    thr['rx'][gnb].append(0)
                    thr['tx'][gnb].append(0)
            for ue in set(rx_df_gnb['RNTI']):
                rx_df_ue = rx_df_gnb[rx_df_gnb['RNTI'] == ue].reset_index()

                # Str to int
                #rx_df = rx_df[~rx_df.isin([np.nan, np.inf, -np.inf]).any(1)]
                #     rx_df = rx_df[~np.isnan(rx_df)]
                rx_df_ue['rxBytes'] = pd.to_numeric(
                    rx_df_ue.rxBytes, errors='coerce', downcast="float")
                thr_rx_entry = rx_df_ue['rxBytes'].sum(
                ) / (app_run_time) * 8 / 1e6  # [Mbit/s]
                rx_df_ue['txBytes'] = pd.to_numeric(
                    rx_df_ue.txBytes, errors='coerce', downcast="float")
                thr_tx_entry = rx_df_ue['txBytes'].sum(
                ) / (app_run_time) * 8 / 1e6  # [Mbit/s]

                thr['rx'][gnb].append(thr_rx_entry)
                thr['tx'][gnb].append(thr_tx_entry)

    return thr


def get_phy_system_throughput(campaign, results, tracename, flow):
    thr_avg = []
    thr_eff_avg = []
    for result in results:
        if(check_errors(campaign, result) != 0):
            continue
        result_filenames = campaign.db.get_result_files(result['meta']['id'])
        result_filename = result_filenames[tracename]
        rx_tx_df = pd.read_csv(result_filename, sep='\t', header=0)
        if(rx_tx_df.size == 0):
            #print('Warning: empty trace!')
            continue
        # Make sure we are using the proper types
        rx_tx_df = rx_tx_df.astype(
            {'DL/UL': 'string', 'time': 'float', 'tbSize': 'int32', 'corrupt': 'int32'})
        # Look just at DL or UL packets
        rx_df = rx_tx_df[rx_tx_df['DL/UL'] == flow]
        # Keep just data received before end of TX time
        #rx_df = rx_df[rx_df['tx_rx_time'] < app_end*1e9]
        thr_entry = rx_df['tbSize'].sum(
        ) / (rx_df['time'].iat[-1] - rx_df['time'].iat[0])  # [Byte/s]
        # Remove corrupt TBs
        rx_df_eff = rx_df[rx_df['corrupt'] == 0]
        thr_eff_entry = rx_df_eff['tbSize'].sum(
        ) / (rx_df['time'].iat[-1] - rx_df['time'].iat[0])  # [Byte/s]

        thr_eff_avg.append(thr_eff_entry/1e6*8)
        thr_avg.append(thr_entry/1e6*8)  # [Mbit/s]

        #print (thr_avg[-1])

    #out['avg'] = stat.mean(thr_avg)
    out = np.mean(thr_eff_avg)
    #out['std_dev'] = stat.stdev(thr_avg)
    #out['std_dev_eff'] = stat.stdev(thr_eff_avg)

    return out


def get_phy_system_throughput_samples(campaign, results, tracename, flow, runtime):
    thr_eff_avg = {}
    numUes = {}
    # find out the number of gnbs and initialize the dictionary
    first_res = next(iter(results))
    temp_fn = campaign.db.get_result_files(first_res['meta']['id'])[tracename]
    temp_df = pd.read_csv(temp_fn, sep='\t', header=0)
    temp_df = temp_df.astype(
        {'DL/UL': 'string', 'time': 'float', 'tbSize': 'int32', 'corrupt': 'int32'})
    for gnb in set(temp_df['cellId']):
        thr_eff_avg[gnb] = []
        numUes[gnb] = []

    for result in results:
        if(check_errors(campaign, result) != 0):
            continue
        result_filenames = campaign.db.get_result_files(result['meta']['id'])
        result_filename = result_filenames[tracename]
        rx_tx_df = pd.read_csv(result_filename, sep='\t', header=0)
        if(rx_tx_df.size == 0):
            #print('Warning: empty trace!')
            continue
        # Make sure we are using the proper types
        rx_tx_df = rx_tx_df.astype(
            {'DL/UL': 'string', 'time': 'float', 'tbSize': 'int32', 'corrupt': 'int32'})
        # Look just at DL or UL packets
        rx_df = rx_tx_df[rx_tx_df['DL/UL'] == flow]
        # Keep just data received before end of TX time
        # subdivide by cell id and imsi well
        for gnb in set(rx_df['cellId']):
            rx_df_gnb = rx_df[rx_df['cellId'] == gnb]
            numUes[gnb].append(len(set(rx_df_gnb['rnti'])))
            for ue in set(rx_df_gnb['rnti']):
                rx_df_ue = rx_df_gnb[rx_df_gnb['rnti'] == ue].reset_index()
                # thr_entry = rx_df_ue['tbSize'].sum(
                # ) / (rx_df_ue['time'].iat[-1] - rx_df_ue['time'].iat[0])  # [Byte/s]
                # Remove corrupt TBs
                rx_df_eff = rx_df_ue[rx_df_ue['corrupt'] == 0]
                thr_eff_entry = rx_df_eff['tbSize'].sum(
                ) / runtime  # [Byte/s]
                thr_mb_per_s = thr_eff_entry/1e6*8
                thr_eff_avg[gnb].append(thr_mb_per_s)

    return thr_eff_avg, numUes


def get_e2e_latency_samples(campaign, results, tracename):
    lat_samples = []
    empty = 0
    for result in results:
        if (check_errors(campaign, result) != 0):
            continue

        result_filename = campaign.db.get_result_files(
            result['meta']['id'])[tracename]
        app_df = pd.read_csv(result_filename, sep=' ',
                             lineterminator='\n', skip_blank_lines=True)

        if (app_df.size == 0):
            continue

        rx_df = app_df[app_df['rx_tx'] == 'Rx']
        rx_df = rx_df.astype({'delay': 'int'})
        delays = rx_df['delay'] / 1e6  # [ns -> ms]
        lat_samples.append(np.array(delays))

    return np.concatenate(lat_samples).ravel()

def get_e2e_latency_samples_ue(campaign, results, tracename, portRx):
    lat_samples = []
    empty = 0
    for result in results:
        if (check_errors(campaign, result) != 0):
            continue

        result_filename = campaign.db.get_result_files(
            result['meta']['id'])[tracename]
        app_df = pd.read_csv(result_filename, sep=' ',
                             lineterminator='\n', skip_blank_lines=True)

        if (app_df.size == 0):
            continue

        rx_df = app_df[app_df['rx_tx'] == 'Rx']
        rx_df = rx_df[rx_df['port_to'] == portRx]
        rx_df = rx_df.astype({'delay': 'int'})
        delays = rx_df['delay'] / 1e6  # [ns -> ms]
        lat_samples.append(np.array(delays))

    return np.concatenate(lat_samples).ravel()


def getTxSinrSamples(cam, results, tracename, ul_or_dl='UL'):
    sinr = []
    for result in results:
        if (check_errors(cam, result) != 0):
            continue
        result_filenames = cam.db.get_result_files(result['meta']['id'])
        result_filename = result_filenames[tracename]
        # IAB trace files are often ill-formatted. Identufy culprit and run iab_phy_preprocessing
        try:
            rx_df = pd.read_csv(result_filename, sep='\t', lineterminator='\n', skiprows=1,
                                names=['dlUl', 'time', 'frame', 'subf', 'slot',  'firstSim', 'simNum', 'cid', 'rnti', 'ccId', 'tb', 'mcs',
                                       'rv', 'sinr', 'corr', 'tbler'])
        except:
            print(f'{result_filename} is ill-formatted!')
        # Str to float
        rx_df = rx_df[rx_df['dlUl'] == ul_or_dl]
        data = pd.to_numeric(rx_df["sinr"], errors='coerce', downcast="float")
        sinr.extend(data)

    return sinr

def getTxSinrSingleUeSamples(cam, results, tracename, ul_or_dl='UL', ueIndex=1):
    sinr = []
    for result in results:
        if (check_errors(cam, result) != 0):
            continue
        result_filenames = cam.db.get_result_files(result['meta']['id'])
        result_filename = result_filenames[tracename]
        # IAB trace files are often ill-formatted. Identufy culprit and run iab_phy_preprocessing
        try:
            rx_df = pd.read_csv(result_filename, sep='\t', lineterminator='\n', skiprows=1,
                                names=['dlUl', 'time', 'frame', 'subf', 'slot',  'firstSim', 'simNum', 'cid', 'rnti', 'ccId', 'tb', 'mcs',
                                       'rv', 'sinr', 'corr', 'tbler'])
        except:
            print(f'{result_filename} is ill-formatted!')
        # Str to float
        rx_df = rx_df[rx_df['dlUl'] == ul_or_dl]
        rx_df = rx_df[rx_df['rnti'] == ueIndex]
        rx_df = rx_df[rx_df['cid'] == 1]
        data = pd.to_numeric(rx_df["sinr"], errors='coerce', downcast="float")
        sinr.extend(data)

    return sinr

def getPhyOccupancyVsDepth(campaign, results, phy_tracename, slot_duration, app_start_sec, sim_duration_dec):

    out_df = pd.DataFrame(columns=['depth', 'metric'])
    max_num_sym = ((sim_duration_dec - app_start_sec )/slot_duration) * 1000 * NUM_DATA_SYM_PER_SLOT

    for result in results:
        # if (check_errors(campaign, result) != 0):
        #     continue

        run_df = pd.DataFrame(columns=['depth', 'metric'])

        result_filenames = campaign.db.get_result_files(result['meta']['id'])
        result_filename = result_filenames[phy_tracename]
        # IAB trace files are often ill-formatted. Identufy culprit and run iab_phy_preprocessing
        try:
            phy_df = pd.read_csv(result_filename, sep='\t', lineterminator='\n', skiprows=1,
                                names=['dlUl', 'time', 'frame', 'subf', 'slot',  'firstSim', 'simNum', 'cid', 'rnti', 'ccId', 'tb', 'mcs',
                                       'rv', 'sinr', 'corr', 'tbler'])
        except:
            print(f'{result_filename} is ill-formatted!')

        phy_df = phy_df.astype({'simNum': 'int'})
        phy_df = phy_df.loc[phy_df['time'] > app_start_sec]

        print (set(phy_df['cid']))
        for cid in set(phy_df['cid']):
            cid_df = phy_df.loc[phy_df['cid'] == cid]
            num_tx_sim = cid_df['simNum'].sum ()
            occ = num_tx_sim / max_num_sym

        out_df = pd.concat(
            [out_df, run_df.groupby('depth').mean().reset_index()])

    return out_df.groupby('depth').mean().reset_index()

def get_e2e_avg_phy_occupancy(campaign, results, phy_tracename, slot_duration, app_start_sec, sim_duration_dec):

    occ_avg = []
    max_num_sym = ((sim_duration_dec - app_start_sec )/slot_duration) * 1000 * NUM_DATA_SYM_PER_SLOT

    for result in results:
        # if (check_errors(campaign, result) != 0):
        #     continue

        result_filenames = campaign.db.get_result_files(result['meta']['id'])
        result_filename = result_filenames[phy_tracename]

        try:
            phy_df = pd.read_csv(result_filename, sep='\t', lineterminator='\n', skiprows=1,
                                names=['dlUl', 'time', 'frame', 'subf', 'slot',  'firstSim', 'simNum', 'cid', 'rnti', 'ccId', 'tb', 'mcs',
                                       'rv', 'sinr', 'corr', 'tbler'])
        except:
            print(f'{result_filename} is ill-formatted!')

        phy_df = phy_df.astype({'simNum': 'int'})
        phy_df = phy_df.loc[phy_df['time'] > app_start_sec]
        
        occ = 0

        print (set(phy_df['cid']))
        for cid in set(phy_df['cid']):
            cid_df = phy_df.loc[phy_df['cid'] == cid]
            num_tx_sim = cid_df['simNum'].sum ()
            occ += num_tx_sim / max_num_sym

        occ_avg.append(occ)
    return sum(occ_avg) / len(occ_avg)


def get_e2e_ue_phy_occupancy(campaign, results, phy_tracename, slot_duration, app_start_sec, sim_duration_dec):

    occ_list = []
    max_num_sym = ((sim_duration_dec - app_start_sec )/slot_duration) * 1000 * NUM_DATA_SYM_PER_SLOT

    for result in results:
        # if (check_errors(campaign, result) != 0):
        #     continue

        result_filenames = campaign.db.get_result_files(result['meta']['id'])
        result_filename = result_filenames[phy_tracename]

        try:
            phy_df = pd.read_csv(result_filename, sep='\t', lineterminator='\n', skiprows=1,
                                names=['dlUl', 'time', 'frame', 'subf', 'slot',  'firstSim', 'simNum', 'cid', 'rnti', 'ccId', 'tb', 'mcs',
                                       'rv', 'sinr', 'corr', 'tbler'])
        except:
            print(f'{result_filename} is ill-formatted!')

        phy_df = phy_df.astype({'simNum': 'int'})
        phy_df = phy_df.loc[phy_df['time'] > app_start_sec]

        occ = 0

        for cid in set(phy_df['cid']):
            cid_df = phy_df.loc[phy_df['cid'] == cid]
            num_tx_sim = cid_df['simNum'].sum ()
            occ += num_tx_sim / max_num_sym

        occ_list.append(occ)
    return occ_list

