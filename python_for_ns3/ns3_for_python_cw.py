import matplotlib.pyplot as plt
import pandas as pd

df1 = pd.read_table("bytes_TcpHybla.dat", sep="\s+",
                    names=["time", "window_size"])
df2 = pd.read_table("bytes_TcpNewReno.dat", sep="\s+",
                    names=["time", "window_size"])
df3 = pd.read_table("bytes_TcpScalable.dat", sep="\s+",
                    names=["time", "window_size"])
df4 = pd.read_table("bytes_TcpVegas.dat", sep="\s+",
                    names=["time", "window_size"])
df5 = pd.read_table("bytes_TcpWestwood.dat", sep="\s+",
                    names=["time", "window_size"])


plt.figure(figsize=(14, 10))
plt.plot(df1["time"], df1["window_size"], label="Hybla")
plt.plot(df2["time"], df2["window_size"], label="NewReno")
plt.plot(df3["time"], df3["window_size"], label="Scalable")
plt.plot(df4["time"], df4["window_size"], label="Vegas")
plt.plot(df5["time"], df5["window_size"], label="Westwood")
plt.legend()
plt.xlabel("Time")
plt.ylabel("Bytes recieved(B)")
plt.grid()
plt.savefig("congestion_window.jpg", dpi=1200)
