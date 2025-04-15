import re
import pandas as pd
import numpy as np

# Initial energy values (in µJ)
INITIAL_ENERGY_MAP = {
    "High": 48600000,
    "Low": 16200000,
    "Weak": 32400000
}

def parse_logs(file_path="finaltest.txt"):
    clients = []
    servers = []

    with open(file_path, "r") as f:
        for line in f:
            parts = line.strip().split('\t')
            if len(parts) != 3:
                continue
            try:
                time = int(parts[0])
                mote_id = int(parts[1].replace("ID:", ""))
                values = parts[2].split(",")

                if values[0].startswith("C"):  # Client
                    if len(values) != 7:
                        print(f"Skipping client line (bad format): {line.strip()}")
                        continue
                    typ = values[0][1]  # W, L, H
                    node_id, etx, hop, energy, degree, seq = map(int, values[1:])
                    node_type = "Weak" if typ == 'W' else "Low" if typ == 'L' else "High"
                    clients.append({
                        "mote_id": node_id,
                        "type": node_type,
                        "seq_id": seq,
                        "etx": etx,
                        "hop_count": hop,
                        "energy": energy,
                        "degree": degree,
                        "sent_time": time
                    })

                elif values[0] == "S":  # Server
                    if len(values) != 9:
                        print(f"Skipping server line (bad format): {line.strip()}")
                        continue
                    _, server_id, etx, hop, energy, degree, from_node, seq_id, _ = values
                    servers.append({
                        "from_node": int(from_node),
                        "seq_id": int(seq_id),
                        "received_time": time
                    })
            except Exception as e:
                print(f"Skipping line (parse error): {line.strip()}")
                continue

    servers_df = pd.DataFrame(servers)
    if "from_node" not in servers_df.columns:
        print("\nNo valid server logs found. Check if server output has proper 'S,...' lines.\n")
    return pd.DataFrame(clients), servers_df

def merge_data(clients_df, servers_df):
    df = pd.merge(clients_df, servers_df, how="left", left_on=["mote_id", "seq_id"], right_on=["from_node", "seq_id"])
    df.drop(columns=["from_node"], inplace=True)

    # Latency calculation
    df["latency"] = df["received_time"] - df["sent_time"]
    df["latency"] = df["latency"].apply(lambda x: x if pd.notnull(x) and x >= 0 and x < 100000 else None)

    # Energy consumed
    df["initial_energy"] = df["type"].map(INITIAL_ENERGY_MAP)
    df["energy_consumed"] = df["initial_energy"] - df["energy"]
    df["energy_consumed"] = df["energy_consumed"].apply(lambda x: x if pd.notnull(x) and x >= 0 else None)

    return df

def main():
    clients_df, servers_df = parse_logs("finaltest.txt")
    if clients_df.empty:
        print("No client data found.")
        return

    df = merge_data(clients_df, servers_df)
    df.to_csv("network_data.csv", index=False)

    # Metrics
    pdr = len(df.dropna(subset=["received_time"])) / len(df)
    avg_latency = df["latency"].mean()
    avg_energy = df["energy_consumed"].mean()

    print(f"\nPDR: {pdr*100:.2f}%")
    print(f"Avg Latency: {avg_latency:.2f} ms")
    print(f"Avg Energy Consumed: {avg_energy:.2f} µJ")

if __name__ == "__main__":
    main()

