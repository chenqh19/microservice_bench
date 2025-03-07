import time
from locust import FastHttpUser, task, between, constant_throughput

class QuickstartUser(FastHttpUser):
    wait_time = constant_throughput(20)

    run_time = "30s"

    @task
    def search_hotels(self):
        query_params = {
            "customerName": "John",
            "inDate": "2023-12-01",
            "outDate": "2023-12-02",
            "latitude": "37.7749",
            "longitude": "-122.4194",
            "locale": "en"
        }
        self.client.get("/search", params=query_params, name="search_hotels")
