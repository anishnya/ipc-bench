#!/bin/bash
import subprocess
import re
import json
from grapher import graphMultipleLines
from sklearn.cluster import KMeans
import numpy as np
import os
from scipy import stats

FIFO = "fifo"
SOCKET = "socket"
TCP = "tcp"
SHAREDMEM = "realm"
REALMPRIME = "realmPrime"

BASE_PATH = "/home/anishnya/ipc-bench/benchmarkOutput"
BENCHMARK_DATA_PATH = "/home/anishnya/ipc-bench/benchmarkData"

IPCS = [
    # SHAREDMEM,
    # SOCKET,
    # FIFO,
    REALMPRIME,
]

PATH_MAP = {
    FIFO: f"{BASE_PATH}/fifo/new",
    SOCKET: f"{BASE_PATH}/socket/new",
    TCP: f"{BASE_PATH}/tcp/new",
    SHAREDMEM: f"{BASE_PATH}/realm/new",
    REALMPRIME: f"{BASE_PATH}/realmPrime/new"
}

DATA_PATH_MAP = {
    FIFO: f"{BENCHMARK_DATA_PATH}/fifo",
    SOCKET: f"{BENCHMARK_DATA_PATH}/socket",
    TCP: f"{BENCHMARK_DATA_PATH}/tcp",
    SHAREDMEM: f"{BENCHMARK_DATA_PATH}/realm",
    REALMPRIME: f"{BENCHMARK_DATA_PATH}/realmPrime",
}

RATE_SIZE_MAP = {
    # FIFO: {8192: set([1,2,3,4,5]), 32768: set([1,2,3,4,5]),65536: set([1,2,3])}, #1: set([1,2,15,30,60]), 128: set([1,2,15,30,60]), 1024: set([1,2,15,30,60]), 4096: set([1,2,15,30]), 8192: set([1,5]),
    # SOCKET: {8192: set([1,2,3,4,5,10,20]), 32768: set([1,2,3,4,5,10,20]),65536: set([1,3,5,10])}, #1: set([1,3,5,10,30,50,90]), 128: set([1,3,5,10,30,50,90]), 1024: set([1,3,5,10,30,50]), 4096: set([1,3,5,10,30,50]), 8192: set([1,3,5,10,20]),
    TCP: {1: set([]), 128: set([]), 1024: set([]), 4096: set([]), 8192: set([]), 32768: set([])},
    SHAREDMEM: {1: set([1,5,15,30,90]), 128: set([1,5,15,90,250]), 1024: set([1,2,15,30,90]), 4096: set([1,2,15,30,40,70]), 8192: set([1,3,15,30,50]), 32768: set([1,5,10,30,50]), 65536: set([1,5,10,30,50]), 262144: set([1,5,10,30,50])},
    REALMPRIME: {128: set([1,2,3,4,5,10,15,30,50,70,90,110,130,150,150,170,190,210,230,250, 300, 350, 400, 450,500])}, #8192: set([1,2,3,4,5,10,15,30,50,70,90]), 32768: set([1,3,5,7,10,15,17,20,23,25,30,40,45,50,55, 60, 70, 80]), 65536: set([1,3,5,7,10,15,17,20,23,25,30,40])
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
    1024: "1 KB",
    2048: "2 KB",
    4096: "4 KB",
    6144: "6 KB",
    8192: "8 KB",
    32768: "32 KB",
    65536: "64 KB",
    262144: "256 KB",
    1048576: "1 MB", 
}

BAD_FILES = {}

def filterData(data):
    threshold = 1  # Z-score threshold for outliers

    # Extract first elements and calculate Z-scores
    first_elements = [x[0] for x in data]  # Extract first elements from tuples
    z_scores = stats.zscore(first_elements)

    # Identify outliers based on Z-scores in the first element
    outlier_indices = [i for i, z in enumerate(z_scores) if abs(z) > threshold]

    # Filter data (tuples with outliers in the first element are removed)
    filtered_data = [data[i] for i in range(len(data)) if i not in outlier_indices]
    return filtered_data

def getKMeans(data):
    data = np.array(data)
    kmeans = KMeans(n_clusters=1)
    kmeans.fit(data)

    # Get cluster labels and centroids
    cluster_labels = kmeans.labels_
    centroids = kmeans.cluster_centers_
    print(centroids)
    return centroids[0]

def median(data):
    if not data:
        return None

    # Check if all elements are numbers
    if not all(isinstance(x, (int, float)) for x in data):
        raise TypeError("Input list must contain only numbers")

    # Sort the data
    sorted_data = sorted(data)
    n = len(sorted_data)

    # Handle even and odd number of elements
    if n % 2 == 0:
        median = (sorted_data[n // 2 - 1] + sorted_data[n // 2]) / 2
    else:
        median = sorted_data[n // 2]

    return median

def get_median(data):
    if len(data) % 2 == 0:
        # Even length (average of middle two elements)
        middle_index1 = len(data) // 2 - 1
        middle_index2 = len(data) // 2
        median_first = (data[middle_index1][0] + data[middle_index2][0]) / 2
        meidan_second = (data[middle_index1][1] + data[middle_index2][1]) / 2
        median = (median_first, meidan_second)
    else:
        # Odd length (middle element)
        median_index = len(data) // 2
        median = data[median_index]
    return median

def get_average_per_element(data):
    return [sum(element) / len(element) for element in zip(*data)]

def get_median_per_element(data):
    return [median(element) for element in zip(*data)]

def average_tuples(data):
    data = sorted(data)
    return get_average_per_element(data)

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

def getInfoFilename(filename):
    pattern = r"(\d+)_(\d+)"
    match = re.search(pattern, filename)

    if match:
        size,rate = match.groups()
        return int(size), int(rate)
    else:
        exit(1)

def parseOutput(output, filename):
    nums = []
    for q in [r"Message rate:\s+([0-9]+)", r"Latency =\s*([+-]?\d+\.\d+)"]:
        match = re.search(q, output)

        if match:
            number = float(match.group(1))
            nums.append(number)
        else:
            return [-1, -1]
    
    return nums

def readFile(filename):
    with open(filename, "r") as file:
        return str(file.read())

def getStats(filename, statMap, ipc):
    size, rate = getInfoFilename(filename)
    throughPut, latency = parseOutput(readFile(filename), filename)
    key = f"{size}_{rate}"
    
    if ipc != REALMPRIME:
        if size not in RATE_SIZE_MAP[ipc]:
            return
        
        if rate not in RATE_SIZE_MAP[ipc][size]:
            return
        
        if throughPut == -1 or latency == -1:
            if ipc not in BAD_FILES:
                BAD_FILES.update({ipc: set([])})
            
            BAD_FILES[ipc].add(key)
            return

    if key not in statMap:
        statMap.update({key: []}) 

    # Filter any crazy high latencies
    
    statMap[key].append((throughPut, latency))

def getInfo(ipc):
    directory = PATH_MAP[ipc]
    filenames = os.listdir(directory)
    blocking, nonblocking = split_list(filenames, isBlockingFile)
    maps = []
    
    for dataSet in [blocking, nonblocking]:
        statMap = {}
        statMapPrime = {}

        for filename in dataSet:
            fullPath = os.path.join(directory, filename)
            
            if os.path.isfile(fullPath):
                getStats(fullPath, statMap, ipc)
        
        # Get Averages and fix keys
        for key, value in statMap.items():
            if not value:
                continue
            
            size, _ = getInfoFilename(key)
            average =  average_tuples(value)
            
            if size not in statMapPrime:
                statMapPrime.update({size: []})
            
            statMapPrime[size].append((average[0], average[1]))
        
        maps.append(statMapPrime)
    
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
        print(filename)
        with open(filename, "w", encoding="utf-8") as f:
            json.dump(data, f)
        

def showBadFiles():
    def getSize(input):
        size, _ = getInfoFilename(input)
        return size
    
    for key, value in BAD_FILES.items():
        vals = list(value)
        vals = sorted(vals, key=getSize)
        print(f"{key} : {vals}")

def runBenchmarks():
    for ipc in IPCS:
        resultMaps = getInfo(ipc)
        block = True
        
        for mapData in resultMaps:
            resultMap = parseResultMap(mapData)
            dumpData(resultMap, ipc, block)
            block = False

    showBadFiles()
    
if __name__ == "__main__":
   runBenchmarks()