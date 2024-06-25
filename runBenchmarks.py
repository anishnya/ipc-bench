#!/bin/bash
import subprocess
import re
import json
from grapher import graphMultipleLines
import os

FIFO = "fifo"
SOCKET = "socket"
TCP = "tcp"
SHAREDMEM = "realm"

BASE_PATH = "/home/anishnya/ipc-bench/benchmarkOutput"
BENCHMARK_DATA_PATH = "/home/anishnya/ipc-bench/benchmarkData"

IPCS = [
    SHAREDMEM
]

PATH_MAP = {
    FIFO: f"{BASE_PATH}/fifo",
    SOCKET: f"{BASE_PATH}/socket",
    TCP: f"{BASE_PATH}/tcp",
    SHAREDMEM: f"{BASE_PATH}/realm",
}

DATA_PATH_MAP = {
    FIFO: f"{BENCHMARK_DATA_PATH}/fifo",
    SOCKET: f"{BENCHMARK_DATA_PATH}/socket",
    TCP: f"{BENCHMARK_DATA_PATH}/tcp",
    SHAREDMEM: f"{BENCHMARK_DATA_PATH}/realm",
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
    8144: "8 kB",
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
    pattern = r"(\d+)_(\d+)"
    match = re.search(pattern, filename)

    if match:
        size, _ = match.groups()
        return int(size)
    else:
        exit(1)

def parseOutput(output):
    nums = []
    for q in [r"Message rate:\s+([0-9]+)", r"Latency =\s*([+-]?\d+\.\d+)"]:
        match = re.search(q, output)

        if match:
            number = float(match.group(1))
            nums.append(number)
        else:
            print("Number not found in the string.")
    
    return nums

def readFile(filename):
    with open(filename, "r") as file:
        return str(file.read())

def getStats(filename, statMap):
    size = int(getSizeInfoFilename(filename))
    throughPut, latency = parseOutput(readFile(filename))

    if size not in statMap:
        statMap.update({size: []}) 

    # Filter any crazy high latencies
    if latency < 300.0:
        statMap[size].append((throughPut, latency))

def getInfo(ipc):
    directory = PATH_MAP[ipc]
    filenames = os.listdir(directory)
    blocking, nonblocking = split_list(filenames, isBlockingFile)
    maps = []
    
    for dataSet in [blocking, nonblocking]:
    
        statMap = {}

        for filename in dataSet:
            fullPath = os.path.join(directory, filename)
        
            if os.path.isfile(fullPath):
                getStats(fullPath, statMap)
    
        maps.append(statMap)
    
    return maps

def sort_by_first(item):
    return item[0]

def parseResultMap(resultMap):
    resultMap = [[key, value] for key, value in sorted(resultMap.items())]
    
    for infoObj in resultMap:        
        infoObj[0] = LABEL_MAP[infoObj[0]]
        infoObj[1] = sorted(infoObj[1], key=sort_by_first)
    
    return resultMap

def dumpData(resultMap, ipc, blockFile):
    addendum = "_block" if blockFile else ""
    
    for size, data in resultMap:       
        filename = "{0}/{1}{2}.json".format(DATA_PATH_MAP[ipc], size, addendum)
        with open(filename, "w", encoding="utf-8") as f:
            json.dump(data, f)
    

def runBenchmarks():
   for ipc in IPCS:
        resultMaps = getInfo(ipc)
        block = True
        
        for mapData in resultMaps:
            resultMap = parseResultMap(mapData)
            dumpData(resultMap, ipc, block)
            block = False
        
        # graph when done
        # graphMultipleLines("Throughput (RPC/sec)", "Latency (us)", ipc, resultMap)

if __name__ == "__main__":
   runBenchmarks()