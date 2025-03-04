
# build images
sudo docker compose build

# install wrk2
yes | sudo apt-get install luarocks
yes | sudo luarocks install luasocket
yes | sudo luarocks install dkjson
cd ../examples/wrk2
make clean
make


./wrk2/wrk -D exp -t 100 -c 100 -d 50 -L -s ./wrk_scripts/scripts/hotel-reservation/mixed-workload_type_1.lua http://localhost:12345 -R 2000