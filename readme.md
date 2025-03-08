This is a simple reservation service that allows you to reserve a room in a hotel.

All the data are hardcoded in the service.

To run the service, you need to install docker and run the following command:

```bash
sudo docker compose build
sudo docker compose up -d
```

To generate workload, please use locust.

Note: intel machines may have to run with virtual environment to run python. To install virtual environment, please run the following command:

```bash
python3 -m venv venv
source venv/bin/activate
pip install locust
```

To generate workload, please run the following command:
```bash
locust --host=http://localhost:50050 --headless -u 2000 -r 1000 --run-time 30s --stop-timeout 0 --processes 50
```

Debugging:

We suggest monitoring the service with docker stats.

```bash
sudo docker stats
```

If one of the service takes too much CPU, it means all threads are busy in some downstream service. You need to increase the number of threads for that service.





