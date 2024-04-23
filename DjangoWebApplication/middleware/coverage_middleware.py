from typing import Any
from coverage import Coverage


class CoverageMiddleware:
    def __init__(self, get_response) -> None:
        self.get_response = get_response
        self.cov = Coverage(branch=True, config_file=".coveragerc")

    def __call__(self, request) -> Any:
        self.cov.start()       
        if (True):
            print("Middleware")
        response = self.get_response(request)
        print("Middleware")
        self.cov.stop()
        self.cov.save()

        return response
        