import argparse
from os import listdir
from os.path import isfile, join
from datetime import datetime

parser = argparse.ArgumentParser()
parser.add_argument('--log_dir', type=str, help='log dir to analyse')
args = parser.parse_args()

onlyfiles = [f for f in listdir(args.log_dir) if isfile(join(args.log_dir, f)) and f.endswith('.txt')]
print(onlyfiles)
for file in onlyfiles:
    opts = file.split('_')
    replicas = int(opts[1])
    down = int(opts[3])
    clients = int(opts[5])
    mean = int(opts[7].split('.')[0])
    with open(join(args.log_dir, file), 'r') as f:
        sents = {}
        recs = {}
        for line in f:
            if 'Received req' in line or 'Sending req' in line:
                client_id = line.split(' ')[5]
                req_id = line.split(':')[-2].split(' ')[-1]
                timestamp = line.split(' ')[4]
                timestamp = datetime.strptime(timestamp, '%H:%M:%S,%f')
                if 'Received req' in line:
                    recs[f"{client_id}_{req_id}"] = timestamp
                else:
                    sents[f"{client_id}_{req_id}"] = timestamp
                # print(f"{replicas} {down} {clients} {mean} {client_id} {req_id} {timestamp}")
    # print(recs, sents)
    latencies = []
    for key in recs:
        latency = recs[key] - sents[key]
        latencies.append(latency.total_seconds())
    print(sum(latencies)/len(latencies))
