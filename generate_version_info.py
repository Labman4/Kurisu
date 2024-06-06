import datetime

build_timestamp = datetime.datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ")

with open("version.txt", "w") as f:
    f.write(build_timestamp)