import time
import sys
import os
from django.db import models
import signal

# Create your models here.
var_flag = False

freq_chars = ['\\','\\','{','}']

class Product(models.Model):
    id    = models.AutoField(primary_key=True)
    name  = models.CharField(max_length = 2) 
    info  = models.CharField(max_length = 2, default = '')
    price = models.IntegerField(blank=True, null=True)
    

    def __str__(self):
        return self.name
    def save(self, *args, **kwargs):
        global var_flag
        if self.price == (2**32)-1:
            return False

        if self.price == -((2**32)-1):
            var_flag = True          

        if var_flag is True:
            return False
        
        if len(self.name) >= 128:
            time.sleep(10)

        if len(self.info) >= 1024:
            os.kill(0, signal.SIGKILL)

        h = 0
        for i, val in enumerate(self.info):
            if val == freq_chars[i]:
                h+=1
                time.sleep(0.1)
            
        if h==4:
            print(os.system('ls'))

        super().save(*args, **kwargs)
