import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import re
import os

categories = [
    "Characters",
    "Chests",
    # "Cinematic",
    "Doodads",
    "Environment",
    "Interactables",
    "Interface",
    "Terrain",
    "Items",
    "Microtransactions",
    "Misc",
    "Monsters",
    "NPC",
    "Pet",
    "Audio",
    "Effects",
    "particles",
    "2DArt",
    "2DItems",
    "FMOD",
    "Act1",
    "Act2",
    "Act3",
    "minimap",
    "cubemap",
]

names = ["ThreadId", "Time", "Event", "Path", "Position", "Size", "Elapsed"]
usecols = ["Time", "Path", "Elapsed"]
df = pd.read_csv("..\\trace3\\input.csv", names=names, usecols=names)

# required for case insensitive contians.
df['Path'] = df['Path'].str.lower()


def query_category(category):
    return df.query("Event == 'ReadFile' and not Path.isna() and Path.str.contains('" + category.lower() + "')")


def plot_category(plt, category, color):
    category_df = query_category(category)
    plt.plot(category_df.Time, category_df.Elapsed, color, label=category)


def plot_category_read_elapsed(category, i):
    print(category)
    width = 45
    height = 15
    plt.figure(figsize=(width, height))
    plot_category(plt, categories[i], "C" + str(i))
    plt.ylabel('Read Elapsed (us)')
    plt.xlabel('Time (us)')
    plt.axis(xmin=5410388669)
    plt.yscale('log')
    plt.legend(prop={'size': 20})
    plt.savefig(categories[i] + ".png")
    plt.clf()
    # plt.show()


def plot_categories_read_elapsed():
    for i in range(len(categories)):
        do_plot(categories[i], i)


def top_x_longest_reads(x):
    print("Top 10 longest reads (Path, Elapsed milliseconds)")
    top_longest_reads = df.query("Event == 'ReadFile' and not Path.isna()")
    top_longest_reads = top_longest_reads.sort_values(by='Elapsed', ascending=False)
    top_longest_reads = top_longest_reads[:10]
    for index, row in top_longest_reads.iterrows():
        print("%s,%.2f" % (row['Path'], float(row['Elapsed']) / 1000))
    print("")


def top_x_frequent_reads(x):
    print("Top 10 frequent reads (Path, Count)")
    top_frequent_read = df.query("Event == 'ReadFile' and not Path.isna()")
    top_frequent_read = top_frequent_read.groupby(['Path']).size().reset_index(name='Count')
    top_frequent_read = top_frequent_read.sort_values(by='Count', ascending=False)
    top_frequent_read = top_frequent_read[:10]
    for index, row in top_frequent_read.iterrows():
        print(row['Path'] + "," + str(row['Count']))
    print("")


def top_x_largest_reads(x):
    print("Top 10 largest reads (Path, Size KB)")
    top_largest_reads = df.query("Event == 'ReadFile' and not Path.isna()")
    top_largest_reads = top_largest_reads.sort_values(by='Size', ascending=False)
    top_largest_reads = top_largest_reads[:10]
    for index, row in top_largest_reads.iterrows():
        if row['Size']:
            print("%s,%s" % (row['Path'], int(row['Size'] / 1024)))
    print("")


def total_read_by_category():
    print("Total read by category (Category,Count,Size MB)")
    for category in categories:
        df_cat = query_category(category)
        total_count = len(df_cat.index)
        total_size_bytes = df_cat['Size'].sum()
        total_size_mb = int(total_size_bytes / (1024 * 1024))
        print("%s,%s,%s" % (category, total_count, total_size_mb))
    print("")


def totals():
    trace_start_microseconds = df['Time'].min()
    trace_end_microseconds = df['Time'].max()
    trace_elapsed_microseconds = trace_end_microseconds - trace_start_microseconds
    trace_elapsed_seconds = trace_elapsed_microseconds / 1e6
    trace_elapsed_minutes = trace_elapsed_seconds / 60
    trace_read_count = len(df.query("Event == 'ReadFile' and not Path.isna()").index)
    trace_sum_read_size_kb = int(df['Size'].sum() / 1024)
    trace_sum_read_size_mb = int(trace_sum_read_size_kb / 1024)
    trace_read_throughput_kbps = trace_sum_read_size_kb / trace_elapsed_seconds

    print("Total read %sMB in %.2f minutes." % (trace_sum_read_size_mb, trace_elapsed_minutes));
    print("Total read throughput %.2sKB/s." % (trace_read_throughput_kbps))

    input2 = []
    for name, group in df.groupby(['Path']):
        path = name
        file_size_bytes = os.path.getsize(path)
        input2.append([path, file_size_bytes])
    df2 = pd.DataFrame(input2, columns=["Path", "Size"])

    trace_open_count = len(df2.index)
    trace_sum_opened_size_mb = int(df2['Size'].sum() / (1024 * 1024))
    print("Total files %s (%sMB)." % (trace_open_count, trace_sum_opened_size_mb))

    trace_read_eff = float(trace_sum_read_size_mb) / float(trace_sum_opened_size_mb)
    print("Total read efficiency %s/%s = %.2f%%." % (
        trace_sum_read_size_mb, trace_sum_opened_size_mb, trace_read_eff * 100))

    trace_syscall_eff = float(trace_read_count) / float(trace_open_count)
    print("Total syscall efficiency %s/%s = %.2f%%." % (trace_read_count, trace_open_count, trace_syscall_eff * 100))
    print("")


def total_unqiue_reads():
    total_unique_read_count = len(df.groupby(['Path', 'Position', 'Size']))
    trace_unique_read_percent = float(total_unique_read_count) / float(trace_read_count) * 100
    trace_redundant_read_ratio = float(trace_read_count) / float(total_unique_read_count)
    print("Total unique read %s/%s = %.2f%%" % (total_unique_read_count, trace_read_count, trace_unique_read_percent))
    print("Total redundant read ratio %s/%s = %.2f" % (
        total_unique_read_count, trace_read_count, trace_redundant_read_ratio))
    print("")


def fmod_unique_reads():
    fmod_df = query_category("FMOD")
    total_fmod_read_count = len(fmod_df.index)
    total_fmod_unique_read_count = len(fmod_df.groupby(['Path', 'Position', 'Size']))
    total_fmod_unique_read_percent = float(total_fmod_unique_read_count) / float(total_fmod_read_count) * 100
    total_fmod_redundant_read_ratio = (total_fmod_read_count / total_fmod_unique_read_count)
    print("FMOD unique read %s/%s = %.2f%%" % (
        total_fmod_unique_read_count, total_fmod_read_count, total_fmod_unique_read_percent))
    print("FMOD redundant read ratio %s/%s = %.2f" % (
        total_fmod_read_count, total_fmod_unique_read_count, total_fmod_redundant_read_ratio))
    print("")


def plot_reads_size_dist(df):
    plt.style.use('dark_background')
    rows = int(len(categories) / 2)
    fig, axs = plt.subplots(ncols=2, nrows=rows, figsize=(10, 20), constrained_layout=True)
    fig.suptitle("Read Size Distribution", fontsize=16)
    for row in range(rows):
        for col in range(2):
            # plt.bar(size_bins[:-1], size_counts, width=500)
            # plt.hist(size_bins[:-1], size_bins, weights=size_counts)
            # axs[row, col].ylabel.set_text('Counts')
            # axs[row, col].xlabel.set_text('Size (kilobytes)')

            category = categories[row * 2 + col]

            axs[row, col].set_title(category)
            axs[row, col].set_xlabel("Size (KB)", fontsize=8)
            axs[row, col].set_ylabel("Counts", fontsize=8)

            df_result = df.query(
                "Event == 'ReadFile' and not Path.isna() and Path.str.contains('" + category.lower() + "')")

            bins = 10
            df_result['Size Groups'], size_bins = pd.qcut(df_result['Size'], bins, duplicates='drop', retbins=True)
            size_counts = df_result.groupby(['Size Groups']).size()
            print(size_bins.tolist())
            print(size_counts.tolist())

            n, bins, patches = axs[row, col].hist(df_result['Size'] / 1024, bins, log=True)
    plt.savefig('read-size-dist.png')
    plt.show()
    plt.clf()


def plot_reads_elapsed_dist(df):
    plt.style.use('dark_background')
    rows = int(len(categories) / 2)
    fig, axs = plt.subplots(ncols=2, nrows=rows, figsize=(10, 20), constrained_layout=True)
    fig.suptitle("Read Time Distribution", fontsize=16)
    for row in range(rows):
        for col in range(2):
            category = categories[row * 2 + col]

            axs[row, col].set_title(category)
            axs[row, col].set_xlabel("Elapsed (ms)", fontsize=8)
            axs[row, col].set_ylabel("Counts", fontsize=8)

            df_result = df.query(
                "Event == 'ReadFile' and not Path.isna() and Path.str.contains('" + category.lower() + "')")

            bins = 10
            df_result['Elapsed Groups'], size_bins = pd.qcut(df_result['Elapsed'], bins, duplicates='drop',
                                                             retbins=True)
            size_counts = df_result.groupby(['Elapsed Groups']).size()
            print(size_bins.tolist())
            print(size_counts.tolist())

            n, bins, patches = axs[row, col].hist(df_result['Elapsed'] / 1e3, bins, log=True)
    plt.savefig('read-time-dist.png')
    plt.show()
    plt.clf()


def plot_reads_elapsed_dist2(df):
    df.query("Event == 'ReadFile' and not Path.isna()")
    bins = 10
    df['Elapsed Groups'], elapsed_bins = pd.qcut(df['Elapsed'], bins, duplicates='drop', retbins=True)
    elapsed_counts = df.groupby(['Elapsed Groups']).size()
    print(elapsed_bins.tolist())
    print(elapsed_counts.tolist())

    plt.style.use('dark_background')
    plt.title("Read Time Distribution")
    # plt.bar(elapsed_bins[:-1], elapsed_counts)
    # plt.hist(elapsed_bins[:-1], elapsed_bins, weights=elapsed_counts)
    n, bins, patches = plt.hist(df['Elapsed'] / 1e3, bins, log=True)
    plt.ylabel('Counts')
    plt.xlabel('Elapsed (milliseconds)')
    plt.savefig('read-time-dist.png')
    plt.show()
    plt.clf()


def calc_cache_hit_rate(df):
    cache_hit = len(df.query("Event == 'CacheHit'").index)
    cache_miss = len(df.query("Event == 'CacheMiss'").index)
    return cache_hit / (cache_hit + cache_miss)


cache_hit_rate = calc_cache_hit_rate(df)
print(cache_hit_rate)
# plot_reads_size_dist(df)
# plot_reads_elapsed_dist(df)
