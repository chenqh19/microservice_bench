import time
from locust import FastHttpUser, task, between, constant_throughput

class HotelUser(FastHttpUser):
    wait_time = constant_throughput(20)

    run_time = "30s"

    # You can adjust these weights to change the proportion of different requests
    TASK_WEIGHTS = {
        "search": 40,
        "recommend": 30,
        "user": 15,
        "reservation": 15
    }

    @task(TASK_WEIGHTS["search"])
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

    @task(TASK_WEIGHTS["recommend"])
    def get_recommendations(self):
        query_params = {
            "latitude": "37.7749",
            "longitude": "-122.4194",
            "require": "near_city",
            "locale": "en"
        }
        self.client.get("/recommend", params=query_params, name="get_recommendations")

    @task(TASK_WEIGHTS["user"])
    def user_login(self):
        query_params = {
            "username": "Cornell_1",
            "password": "1111111111"
        }
        self.client.get("/user", params=query_params, name="user_login")

    @task(TASK_WEIGHTS["reservation"])
    def make_reservation(self):
        query_params = {
            "customerName": "John",
            "hotelId": "1",
            "inDate": "2023-12-01",
            "outDate": "2023-12-02",
            "roomNumber": "1",
            "username": "Cornell_1",
            "password": "1111111111"
        }
        self.client.get("/reservation", params=query_params, name="make_reservation")
