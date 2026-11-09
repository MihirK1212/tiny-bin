#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

kubectl delete --ignore-not-found -f k8s/gateway.yaml
kubectl delete --ignore-not-found -f k8s/client.yaml
kubectl delete --ignore-not-found -f k8s/server.yaml
kubectl delete --ignore-not-found -f k8s/db.yaml
kubectl delete --ignore-not-found -f k8s/postgres-init.yaml
kubectl delete --ignore-not-found secret tiny-bin-secret

kubectl delete --ignore-not-found pvc postgres-data-postgres-0

minikube stop