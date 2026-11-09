## one-stop soltuioon, use the scripts

```k8s/scripts/run.sh```
```k8s/scripts/cleanup.sh```

## Start minkikube k8s cluster

```
minikube start --driver=docker --cpus=4 --memory=4096
```

## env setup

To load app env from `.env` without committing it:

```
kubectl create secret generic tiny-bin-secret --from-env-file=.env
```

## postgres

- for postgresql we use StatefulSet

- A StatefulSet is a Kubernetes workload controller (similar to a Deployment) designed for applications where individual Pods need stable identities and persistent storage

```
kubectl apply -f k8s/postgres/postgres-init.yaml
kubectl apply -f k8s/postgres/db.yaml
```

-  check if postgres is working

```
kubectl logs postgres-0
```

```
kubectl exec -it postgres-0 -- psql -U tinybin -d tinybin -c "\dn"
kubectl exec -it postgres-0 -- psql -U tinybin -d tinybin -c "\dt tiny_bin.*"
kubectl exec -it postgres-0 -- psql -U tinybin -d tinybin -c "\d tiny_bin.pastes"
```

```
kubectl exec -it postgres-0 -- psql -U tinybin -d tinybin -c \
  "SELECT table_schema, table_name FROM information_schema.tables WHERE table_schema = 'tiny_bin';"
```

## load the docker images into minkube

```
docker compose build
```

```
minikube image load tiny-bin-tiny-bin-server:latest 
```

## start the deployments 

```
kubectl apply -f k8s/server.yaml
```

```
kubectl apply -f k8s/server.yaml
```

```
kubectl apply -f k8s/client.yaml
```

```
kubectl apply -f k8s/gateway.yaml
```