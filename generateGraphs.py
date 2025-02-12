import subprocess
import re
import json
from grapher import graphMultipleLines, graphLine
import os

FIFO = "fifo"
SOCKET = "socket"
TCP = "tcp"
SHAREDMEM = "realm"
REALMPRIME = "realmPrime"
PAPER = "paper"

BASE_PATH = "/home/anishnya/ipc-bench/benchmarkOutput"
BENCHMARK_DATA_PATH = "/home/anishnya/ipc-bench/benchmarkData"
GRAPH_PATH = "/home/anishnya/ipc-bench/graphs"

IPCS = [
    FIFO,
    SOCKET,
    SHAREDMEM,
    REALMPRIME,
]

GRAPH_MAP = {
    FIFO: "Pipe",
    SOCKET: "Socket",
    SHAREDMEM: "Baseline",
    REALMPRIME: "Baseline w/o Copies",
}

PATH_MAP = {
    FIFO: f"{BASE_PATH}/fifo",
    SOCKET: f"{BASE_PATH}/socket",
    TCP: f"{BASE_PATH}/tcp",
    SHAREDMEM: f"{BASE_PATH}/realm",
    REALMPRIME: f"{BASE_PATH}/realmPrime"
}

DATA_PATH_MAP = {
    FIFO: f"{BENCHMARK_DATA_PATH}/fifo",
    SOCKET: f"{BENCHMARK_DATA_PATH}/socket",
    TCP: f"{BENCHMARK_DATA_PATH}/tcp",
    SHAREDMEM: f"{BENCHMARK_DATA_PATH}/realm",
    REALMPRIME: f"{BENCHMARK_DATA_PATH}/realmPrime",
}

GRAPH_PATH = {
    FIFO: "fifo",
    SOCKET: "socket",
    TCP: "tcp",
    SHAREDMEM: "realm",
    REALMPRIME: "realmPrime",
    PAPER: "paper", 
}

LABEL_MAP = {
    1: "1 B",
    8: "8 B",
    16: "16 B",
    32: "32 B",
    64: "64 B",
    128: "128 B",
    256: "256 B",
    512: "512 B",
    1024: "1 kB",
    2048: "2 kB",
    4096: "4 kB",
    6144: "6 kB",
    8192: "8 KB",
    32768: "32 KB",
    65536: "64 KB",
    262144: "256 KB",
    1048576: "1 MB", 
}

def isBlockingFile(filename):
    return "block" in filename

def split_list(data, is_true):
    true_list = []
    false_list = []
    
    for item in data:
        if is_true(item):
            true_list.append(item)
        else:
            false_list.append(item)
    
    return true_list, false_list


def getSizeInfoFilename(filename):
    pattern = r"^(.*?)\.json"
    match = re.search(pattern, filename)

    if match:
        size = match.groups()[0]
        return size
    else:
        exit(1)


def readFile(filename):
    with open(filename, "r") as file:
        dataStr = file.read()
        return json.loads(dataStr)

def convertData(data):
    throughputs = [item[0] for item in data]
    latencies = [item[1] for item in data]    
    return [throughputs, latencies]


def getStats(filename, fullpath, statMap):
    size = getSizeInfoFilename(filename)
    data = readFile(fullpath)
    
    if size not in statMap:
        statMap.update({size: convertData(data)}) 
    
    return statMap

def getInfo(ipc):
    directory = DATA_PATH_MAP[ipc]
    filenames = os.listdir(directory)
    blocking, nonblocking = split_list(filenames, isBlockingFile)
    maps = []

    for dataSet in [blocking, nonblocking]:
        statMap = {}

        for filename in dataSet:
            fullPath = os.path.join(directory, filename)
            if os.path.isfile(fullPath):
                getStats(filename, fullPath, statMap)
    
        maps.append(statMap)

    return maps

def generateGraphs():
   for ipc in IPCS:
        resultMaps = getInfo(ipc)
        blocking = True
        
        for resultMap in resultMaps:
            addendum = "_blocking" if blocking else ""
            desired_keys = ["64 KB", "256 KB"]
            resultMap = {key: value for key, value in resultMap.items() if key in desired_keys}
            filename = "{0}_{1}".format(ipc, addendum)
            graphMultipleLines("Throughput (RPC/sec)", "Latency (us)", f"{GRAPH_PATH[ipc]}/{filename}", resultMap)

            blocking = False

def paperGraphs():
    desired_keys = ["128 B"]
    
    for key in desired_keys:
        graphMap = {}
        
        for ipc in IPCS:
            val = getInfo(ipc)[1][key]
            graphMap.update({GRAPH_MAP[ipc]: val})
            filename = "{0}_all_ipcs".format(key)
            graphMultipleLines("Throughput (RPC/s)", "Latency (us)", f"{GRAPH_PATH[PAPER]}/{filename}", graphMap)

if __name__ == "__main__":
   paperGraphs()