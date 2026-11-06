FROM node:24-alpine AS build

WORKDIR /app

COPY src/client/tiny-bin/package.json src/client/tiny-bin/package-lock.json ./

RUN --mount=type=cache,target=/root/.npm npm ci

COPY src/client/tiny-bin/ ./

RUN npm run build

FROM nginx:1.28-alpine

COPY src/client/tiny-bin/nginx.conf /etc/nginx/conf.d/default.conf
COPY --from=build /app/dist /usr/share/nginx/html

EXPOSE 80
