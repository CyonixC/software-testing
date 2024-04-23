from typing import Any
from coverage import Coverage
import atexit


class CoverageMiddleware:
    def __init__(self, get_response) -> None:
        self.get_response = get_response
        self.cov = Coverage(branch=True, config_file=".coveragerc")
    
    def exit_fn(self):
        self.cov.stop()
        self.cov.save()

    def __call__(self, request) -> Any:
        self.cov.start()       
        atexit.register(self.exit_fn)
        response = self.get_response(request)
        self.cov.stop()
        self.cov.save()

        return response
        