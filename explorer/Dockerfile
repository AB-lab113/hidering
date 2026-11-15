FROM python:3.11.16-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY app.py .
COPY static/ static/
EXPOSE 8081
CMD ["python", "app.py"]
