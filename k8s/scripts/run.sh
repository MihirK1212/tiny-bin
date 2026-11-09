#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

if [[ ! -f .env ]]; then
  echo "missing .env in $ROOT" >&2
  exit 1
fi

minikube start --driver=docker --cpus=4 --memory=4096

docker compose build

minikube image load tiny-bin-tiny-bin-server:latest
minikube image load tiny-bin-tiny-bin-client:latest
minikube image load tiny-bin-tiny-bin-gateway-proxy:latest

kubectl create secret generic tiny-bin-secret \
  --from-env-file=.env \
  --dry-run=client -o yaml | kubectl apply -f -

kubectl apply -f k8s/postgres-init.yaml
kubectl apply -f k8s/db.yaml
kubectl rollout status statefulset/postgres --timeout=300s

kubectl apply -f k8s/server.yaml
kubectl apply -f k8s/client.yaml
kubectl apply -f k8s/gateway.yaml

kubectl rollout status deployment/tiny-bin-server-deployment --timeout=180s
kubectl rollout status deployment/tiny-bin-client-deployment --timeout=180s
kubectl rollout status deployment/tiny-bin-gateway-proxy-deployment --timeout=180s

echo "to start gateway, run this command: minikube service tiny-bin-gateway-proxy"
