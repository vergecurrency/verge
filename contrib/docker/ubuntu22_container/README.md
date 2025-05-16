## Build and start with any other you need

docker build -t my/container_verge .

docker-compose stop && docker-compose down && docker-compose up -d  

docker-compose ps

## Docker settings for Internet from containers (windows)

{
  "registry-mirrors": [],
  "insecure-registries": [],
  "debug": true,
  "experimental": true,
  "iptables" : true,
  "ip-forward" : true,
  "ip-masq" : true,
  "icc" : true
}

## Docker direct access

docker exec -i -t container_verge /bin/bash


For the latest maintained Verge Core Docker: 
[Docker-Verge-Daemon](https://github.com/vergecurrency/Docker-Verge-Daemon)






