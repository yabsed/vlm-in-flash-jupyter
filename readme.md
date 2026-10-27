# SSD의 throughput saturation 현상을 활용한 Selective Weight Loading 개선 방안

SSD에 throughput saturation이라는 현상이 존재한다는 것은 잘 알려져 있다. SSD에 대해 요청 크기를 늘려가며 I/O latency를 측정하면, 어느 순간부터 단위 용량당 latency 증가가 사라지게 된다. 