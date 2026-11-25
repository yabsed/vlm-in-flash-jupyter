# SSD 처리량 포화를 고려한 타일 기반 선택적 가중치 로딩

<style>
.markdown-preview table th:first-child,
.markdown-preview table td:first-child,
.markdown-preview table .katex {
  white-space: nowrap;
}
.markdown-preview table th,
.markdown-preview table td {
  vertical-align: middle;
}
.report-figure {
  width: 80%;
  max-width: 560px;
  margin: 1.1em auto;
  break-inside: avoid;
  page-break-inside: avoid;
}
.report-figure--wide {
  width: 90%;
  max-width: 680px;
}
.report-figure img {
  display: block;
  width: 100%;
  height: auto;
  margin: 0 auto;
}
.report-figure figcaption {
  margin-top: 0.4em;
  font-size: 0.85em;
  line-height: 1.45;
  text-align: left;
}
@media print {
  .report-figure {
    width: 70%;
    max-width: 125mm;
    margin: 0.6em auto;
  }
  .report-figure--wide {
    width: 68%;
    max-width: 140mm;
  }
  .report-measurement-setup p {
    line-height: 1.45;
    margin-bottom: 0.6em;
  }
  .markdown-preview table {
    break-inside: avoid;
    page-break-inside: avoid;
  }
}
</style>

## 1. 연구 배경과 목표

### 1.1 SSD의 구조에 대해 Jeong 등이 진행한 연구 (2013)

Jeong 등(2013)의 연구 결과에 따르면, 플래시 메모리는 HDD와 달리 물리적 위치와 무관하게 데이터를 빠르게 접근할 수 있기 때문에, 랜덤 접근에서 균일하고 빠른 속도를 제공하는 장점이 있다. 그러나 단일 플래시 메모리의 지속 읽기/쓰기 속도는 HDD에 비해 느린데, 이것을 보완하기 위해 대부분의 SSD는 다중 채널(multichannel) 및 다중 웨이(multiway) 구조를 채택한다. 

해당 연구진은 IOMeter를 이용해 HDD와 SSD가 처리하는 request size에 따라 throughput이 어떻게 변화하는지를 측정하였다. SSD의 읽기와 쓰기 작업 모두에서, request size가 증가할수록 througput(단위 시간당 처리 용량)이 증가하다가, 특정 request size를 넘어가면 througput이 증가를 멈추는 현상이 발견되었다. 이것은 request size가 커짐에 따라 SSD가 자신의 다중 채널을 병렬적으로 사용하며 채널 활용도를 최대화한 상태에 도달했기 때문이다. 

해당 연구진은 SSD의 쓰기와 읽기 작업에서 이러한 throughput saturation 현상이 어떻게 달라지는지를 분석했다. SSD의 쓰기 작업에서는 순차 접근이 랜덤 접근보다 throughput이 높게 나타나는 모습을 보였다. 그런데 SSD 읽기 작업에서는 순차 접근과 랜덤 접근이 비슷한 request size에서 throughput이 포화되었고, 포화 상태의 throughput 수준도 유사하였다. 

### 1.2 I/O-Efficient Sparsification에 대해 이영기 등이 진행한 연구(2025)

LLM과 같은 모델을 휴대용 하드웨어에서 실행하는 데 있어서의 난점은, 장치의 DRAM 용량이 모델 전체를 담기에 충분하지 않은 경우가 많다는 것이다. 가중치는 플래시에 저장해두고, 중요할 것으로 예상되는 행들을 선별해 DRAM으로 가져오는 Activation Sparsification 방법론에 대한 연구가 진행되어 왔다. 

이영기 등(2025)의 연구팀은 I/O latency가 request size의 절대적인 양에 비례해서 증가할 것이라는 기존 방법론의 전제 조건에 의문을 제기하였다. 같은 양의 가중치를 읽어도, 선택한 행들이 넓은 범위에 듬성하게 분포해 있다면, throughput이 낮아질 수 있다. VLM과 같이 channel의 중요도가 균일하게 분포한 모델 가중치의 경우에서는 그러한 효과가 더 두드러질 수 있다고 논문에서는 주장한다. 

이에 연구진은 모델 가중치의 행들의 물리적 연속성을 반영하여 읽기 지연시간을 예측할 수 있는 chunk-based latency model을 정의하였고,  중요도-지연시간 기반 선택 방법인 Neuron Chunking을 제안하였다. 

가중치 행의 수를 $N$, 선택 mask를 $M\in\{0,1\}^N$라고 하자. $M_i=1$이면 $i$번째 행을 읽는다. 토큰 수가 $H$인 입력에서 $i$번째 채널의 중요도와 mask가 보존하는 중요도를 아래와 같이 정의한다. 

$$
v_i=\frac1H\sum_{t=1}^{H}|a_{t,i}|,
\qquad
I(M)=\sum_{i=0}^{N-1}v_iM_i
$$

$a_{t, i}$는 토큰 $t$의 채널 $i$번째 activation이다. 

mask $M$에서 선택된 행들을 최대 연속 구간으로 분해하고 그 집합을 $\mathcal C(M)$라 하자. 길이가 $l$인 행 구간을 읽는데 걸리는 latency 비용을 $T[l]$로 정의하자. 원 논문의 chunk-based latency 모델은 다음과 같다. 

$$
\widehat L_{\mathrm{table}}(M)
=\sum_{C\in\mathcal C(M)}T[|C|]
$$

연구팀은 각각의 행 구간 길이 $l$에 대해 충분한 수의 같은 크기 chunk를 읽어 처리량이 안정된 조건에서 $T[l]$를 측정하였다. $\widehat L_{\mathrm{table}}$의 정의를 이용하면 $M$에 해당하는 열들을 플래시로부터 읽어오는 데 걸리는 시간을 대략적으로 근사할 수 있다. 

연구팀은 행 예산 $R$ 아래에서의 단위 추정 지연시간당 보존 중요도를 최대화하는 mask $M$을 찾고자 하는 문제를 제안하였다. 

$$
\max_{\substack{M\in\{0,1\}^N\\1\le\|M\|_1\le R}}
\frac{I(M)}{\widehat L_{\mathrm{table}}(M)}.
$$

논문의 Greedy 알고리즘은 중요도–지연시간 비율이 높은 mask $M$을 구성하기 위해 연속 후보 구간들의 집합 $\mathcal G$를 생성한다. 각 후보 $C\in\mathcal G$에 대해

$$
U(C)=\frac{\sum_{i\in C}v_i}{T[|C|]}
$$

를 계산한다. 후보들을 점수가 높은 순서대로 검토하며, 이미 선택한 행과 겹치지 않고 잔여 예산을 넘지 않는 구간을 선택해 mask를 갱신한다.

시작 행이 $j$이고 길이가 $\ell$인 구간 후보를

$$
C_{j,\ell}=\{j,j+1,\ldots,j+\ell-1\},
\qquad 0\le j\le N-\ell
$$

로 정의하자. 

행 단위로 환산한 최소·최대 후보 길이를 $\ell_{\min},\ell_{\max}$, 길이 증가 간격을 $\Delta\ell$, 시작 위치의 이동 간격 상한을 $J$라 하자. 논문이 후보 집합 $\mathcal G$를 구성하기 위해 고려하는 $(j,\ell)$ 쌍은 다음과 같다.


$$
\begin{aligned}
\ell&=\ell_{\min}+k\Delta\ell\le\min(\ell_{\max},N),\\
j&=m\min(\ell,J)\le N-\ell,
\end{aligned}
\qquad k,m=0,1,2,\ldots
$$

이 규칙은 $\ell>J$일 때 같은 길이의 후보 간 중첩을 허용한다. $\ell_{\max}$는 장치의 처리량 포화 지점을 기준으로 정하고, $\ell_{\min}$, $\Delta\ell$, $J$는 탐색의 세밀함과 계산량을 조절하는 매개변수로 사용한다. 

## 2. 문제 정의와 제안 방법

### 2.1 논문에서 제안한 문제의 실제 최적해

논문이 제시한 Neuron Chunking 문제를 생각해보자. 

$$
\max_{\substack{M\in\{0,1\}^N\\1\le\|M\|_1\le R}}
\frac{I(M)}{\widehat L_{\mathrm{table}}(M)}.
$$

$1\le R\le N$이고, 모든 길이 $1\le\ell\le R$에 대해 $T[\ell]>0$이라 하자. 

행 예산 $1\le\|M\|_1\le R$ 아래에서 임의의 최적 mask $M^\star$를 생각하자. $\mathcal C(M^\star)$의 구간들은 선택된 행들을 분할하므로

$$
\begin{aligned}
\frac{I(M^\star)}{\widehat L_{\mathrm{table}}(M^\star)}
&=
\sum_{C\in\mathcal C(M^\star)}
\underbrace{
\frac{T[|C|]}{\widehat L_{\mathrm{table}}(M^\star)}
}_{w_C}
\underbrace{
\frac{\sum_{i\in C}v_i}{T[|C|]}
}_{U(C)}\\
&=
\sum_{C\in\mathcal C(M^\star)}w_CU(C)
\le
\max_{C\in\mathcal C(M^\star)}U(C).
\end{aligned}
$$

$w_C>0$이고 $\sum_Cw_C=1$이므로, 전체 비율은 각 구간의 비율을 latency로 가중한 평균이다. 따라서 가장 큰 구간 비율을 넘을 수 없다.

$U(C)$가 가장 큰 구간 $C^\star\in\mathcal C(M^\star)$만 남긴 mask를 $\widetilde M$이라 하자. 

$$1\le\|\widetilde M\|_1=|C^\star|\le\|M^\star\|_1\le R$$

이므로 $\widetilde M$도 행 예산을 만족한다. 앞선 부등식에 의해 $\widetilde M$의 비율은 $M^\star$보다 작지 않고, $M^\star$의 최적성에 의해 더 클 수도 없다. 따라서 $\widetilde M$도 최적해다.

따라서 단일 연속 구간으로 이루어진 최적해가 존재하며, 원래 문제는 길이가 $R$ 이하인 모든 연속 구간 중 $U(C)$를 최대화하는 구간을 찾는 문제가 된다. 

$$
\max_{\substack{M\in\{0,1\}^N\\1\le\|M\|_1\le R}}
\frac{I(M)}{\widehat L_{\mathrm{table}}(M)}
=
\max_{\substack{1\le\ell\le R\\0\le j\le N-\ell}}
U(C_{j,\ell}).
$$

오른쪽의 비교 대상은 후보 집합 $\mathcal G$에 한정되지 않는 모든 연속 구간이다. 

이러한 현상은 목표함수가 정확히 $R$개의 행을 선택하는 것이 아니라, 행 수의 상한에 제약을 거는 것으로 느슨하게 정의되어 있기 때문에 발생한다. 따라서 본 연구에서는 정확히 $R$개의 행을 선택하는 더 강한 문제에 대해 논의를 이어가고자 한다.

$$
\boxed{
\max_{\substack{M\in\{0,1\}^N\\\|M\|_1=R}}
\frac{I(M)}{\widehat L(M)}.
}
$$

### 2.2 Two-line 지연 모델

#### 2.2.1 연속 구간의 읽기 비용

SSD의 처리량은 요청 크기가 증가함에 따라 증가하다가 일정 수준에서 포화되는 경향을 보인다. 포화 구간의 행당 읽기 비용을 $\alpha>0$라 두면, 길이가 $\ell$행인 I/O 요청의 지연시간을 다음과 같이 근사할 수 있다.

$$
T[\ell]=\alpha\ell,
\qquad \ell\gg1.
$$

작은 요청에서의 처리량 감소를 함께 반영하기 위해, 본 연구에서는 측정표 $T[\ell]$를 다음의 two-line 모델로 근사한다.

길이가 $\ell$행인 구간의 비용을 $T_{\mathrm{2L}}(\ell)$이라 하자. 행 단위로 환산한 전환점을 $\ell_0>0$, 추가 비용의 계수를 $0\le\lambda\le\alpha$라 두면

$$
\boxed{
\begin{aligned}
T_{\mathrm{2L}}(\ell)
&=\alpha\ell+\lambda(\ell_0-\ell)_+\\
&=
\begin{cases}
(\alpha-\lambda)\ell+\lambda\ell_0,
&0<\ell<\ell_0,\\
\alpha\ell,
&\ell\ge\ell_0.
\end{cases}
\end{aligned}
}
$$

여기서 $(x)_+=\max(x,0)$이며, 읽을 행이 없는 경우는 별도로 $T_{\mathrm{2L}}(0)=0$이라 둔다.

이때 두 직선은 $\ell=\ell_0$에서 연속이며, 행 단위 처리량은

$$
\frac{\ell}{T_{\mathrm{2L}}(\ell)}
=
\frac{1}{\alpha+\lambda(\ell_0/\ell-1)_+}
\le\frac1\alpha
$$

이다. $\lambda>0$이면 전환점 이전에는 길이가 늘수록 처리량이 증가하고, 전환점 이후에는 $1/\alpha$로 일정해진다.

모델의 계수 $\alpha,\lambda$와 전환점 $\ell_0$는 SSD 읽기 비용 측정값에 적합하여 결정한다.

#### 2.2.2 고정된 행 예산에서의 전체 읽기 비용

이제 이 모델을 이용해 정확히 $R$개 행을 선택한 mask $M$의 전체 읽기 비용을 살펴보자. 선택한 행 수가 같더라도 연속 구간의 구성에 따라 비용이 달라지며, two-line 모델에서는 이 차이를 짧은 구간의 추가 비용으로 표현할 수 있다.

선행연구의 구간별 비용을 합산하는 구조에 two-line 모델을 적용하자. $\sum_{C\in\mathcal C(M)}|C|=R$이므로 전체 비용은 다음과 같이 분해된다.

$$
\begin{aligned}
\widehat L_{\mathrm{2L}}(M)
&=\sum_{C\in\mathcal C(M)}T_{\mathrm{2L}}(|C|)\\
&=\alpha R
+\lambda\sum_{C\in\mathcal C(M)}(\ell_0-|C|)_+.
\end{aligned}
$$

따라서 같은 $R$에서 모델 비용의 차이는 전환점보다 짧은 구간들의 길이 부족분으로 결정된다. 각 구간은 길이가 $\ell_0$에 도달할 때까지 추가 비용을 줄일 수 있으며, 그 이후에는 추가 비용이 없다.

이 분해를 이용하면 고정된 행 예산에서의 비용 하한도 얻을 수 있다. 모든 구간의 길이가 $R$ 이하이므로 $\sum_C(\ell_0-|C|)_+\ge(\ell_0-R)_+$이고, 따라서

$$
\widehat L_{\mathrm{2L}}(M)
\ge
\alpha R+\lambda(\ell_0-R)_+
=T_{\mathrm{2L}}(R).
$$

길이 $R$의 단일 구간은 이 하한을 달성한다. $\lambda>0$일 때 $R<\ell_0$이면 단일 구간만 하한을 달성하지만, $R\ge\ell_0$이면 모든 구간의 길이가 $\ell_0$ 이상인 mask가 하한을 달성한다. 즉, 여러 구간으로 나뉘어 있더라도 각 구간이 충분히 길면 모델 비용은 동일한 $\alpha R$이다.

한편 행의 배치를 바꾸면 보존 중요도 $I(M)$도 달라진다. 따라서 다음 절에서는 중요도와 짧은 구간의 추가 비용을 함께 고려하는 다음 문제의 최적해를 구한다.

$$
\max_{\substack{M\in\{0,1\}^N\\\|M\|_1=R}}
\frac{I(M)}
{\displaystyle
\alpha R+
\lambda\sum_{C\in\mathcal C(M)}(\ell_0-|C|)_+
}.
$$

### 2.3 동적계획법을 이용한 최적 mask 탐색

앞 절에서 정의한 two-line 비용 모델 아래에서 정확히 $R$개 행을 선택하는 문제를 생각하자. 표기를 줄이기 위해 $T(\ell)=T_{\mathrm{2L}}(\ell)$, $L(M)=\widehat L_{\mathrm{2L}}(M)$이라 쓰고, 최적 비율을

$$
q^\star=
\max_{\substack{M\in\{0,1\}^N\\\|M\|_1=R}}
\frac{I(M)}{L(M)}
$$

이라 하자. 여기서 $1\le R\le N$, $v_i\ge0$이며, 앞 절의 계수 조건에 의해 모든 가능한 mask의 비용은 $L(M)\ge T(R)>0$이다.

비율의 추정값 $q$를 고정하면, 다음의 합 형태 목적함수를 동적계획법(DP)으로 최대화할 수 있다.

$$
F_R(q)=
\max_{\substack{M\in\{0,1\}^N\\\|M\|_1=R}}
\{I(M)-qL(M)\}.
$$

먼저 고정된 $q$에서 이 문제를 푸는 방법을 설명한 뒤, DP 결과를 이용해 $q^\star$를 찾는 과정을 설명한다.

#### 2.3.1 고정된 $q$에서의 DP

행의 인덱스는 $0,\ldots,N-1$로 두고, 중요도의 누적합을 $P_j=\sum_{i=0}^{j-1}v_i$, $P_0=0$으로 정의하자. $D_{z,r}$는 앞의 $z+r$개 행에서 $z$개를 제외하고 $r$개를 선택했을 때의 $I-qL$의 최댓값이다. 필요한 상태의 범위는 $0\le z\le N-R$, $0\le r\le R$이다.

$z\ge1$인 mask에서 마지막으로 선택하지 않은 행을 기준으로 나누자. 그 행 앞에서 선택한 행 수가 $t$라면, 앞부분에는 제외한 행이 $z-1$개, 선택한 행이 $t$개 있다. 마지막으로 제외한 행의 인덱스는 $z+t-1$이고, 그 뒤의 $[z+t,z+r)$는 길이가 $r-t$인 연속 선택 구간이다.

제외한 행이 앞부분과 마지막 구간을 분리하므로 두 부분의 비용을 합산할 수 있다. 마지막 구간의 중요도는 $P_{z+r}-P_{z+t}$이므로

$$
\boxed{
D_{z,r}
=\max_{0\le t\le r}
\left\{
D_{z-1,t}
+P_{z+r}-P_{z+t}
-qT(r-t)
\right\}.
}
$$

$t=r$이면 mask가 선택하지 않은 행으로 끝나며, 마지막 구간은 비어 있다. 이 경우 $T(0)=0$을 사용한다. 모든 mask는 이러한 분해를 가지므로, 모든 $t$를 비교하는 위 점화식은 최적값을 구한다.

제외한 행이 없으면 앞의 $r$개 행을 모두 선택해야 하므로 초기값과 최종값은

$$
D_{0,r}=P_r-qT(r),
\qquad
F_R(q)=D_{N-R,R}
$$

이다. 최적 mask는 $(z,r)=(N-R,R)$에서 역추적한다. 각 상태에서 최댓값을 만드는 $t$를 찾아 $[z+t,z+r)$를 선택하고, $(z,r)\leftarrow(z-1,t)$로 이동한다. $z=0$에 도달하면 남은 앞의 $r$개 행을 모두 선택한다.

#### 2.3.2 Two-line 구조를 이용한 DP 계산

점화식에서 각 상태마다 모든 $t$를 따로 비교하면 계산량이 커진다. Two-line 모델에서는 마지막 구간을 빈 구간, 짧은 구간, 긴 구간으로 나누어 최댓값 계산을 공유할 수 있다.

정수 길이를 구분하기 위한 경계를

$$
h=\min(\lfloor\ell_0\rfloor,R)
$$

로 두자. $1\le r-t\le h$인 경우에는 짧은 구간의 비용식을, $r-t>h$인 경우에는 긴 구간의 비용식을 사용한다. $\ell_0$가 정수이면 경계에서 두 비용식이 같으므로 그 길이를 짧은 구간 쪽에 포함해도 된다. 비용 계산에 쓰는 전환점 $\ell_0$는 반올림하지 않는다.

점화식에서 $t$에 의존하는 항을

$$
\begin{aligned}
A_{z,t}&=D_{z-1,t}-P_{z+t}+q(\alpha-\lambda)t,\\
B_{z,t}&=D_{z-1,t}-P_{z+t}+q\alpha t
\end{aligned}
$$

로 모으면

$$
D_{z,r}=
\max\left\{
\begin{aligned}
&D_{z-1,r},\\
&P_{z+r}-q\bigl((\alpha-\lambda)r+\lambda\ell_0\bigr)
+\max_{\max(0,r-h)\le t<r}A_{z,t},\\
&P_{z+r}-q\alpha r
+\max_{0\le t<r-h}B_{z,t}
\end{aligned}
\right\}.
$$

세 항은 각각 마지막 구간이 비어 있는 경우, 짧은 경우, 긴 경우에 해당한다. 후보가 없는 범위의 최댓값은 $-\infty$로 둔다.

$r$이 증가할 때 $A_{z,t}$를 조회하는 범위는 폭이 $h$인 이동 구간이며, $B_{z,t}$를 조회하는 범위는 오른쪽 끝이 늘어나는 접두 구간이다. 따라서 이동 구간 최댓값과 누적 최댓값을 이용하면, 고정된 $z$에서 모든 $r$의 값을 $O(R+1)$ 시간에 계산할 수 있다.

DP 표의 크기는 $(N-R+1)(R+1)$이다. 역추적에서도 각 $z$마다 최대 $R+1$개의 후보를 비교하므로, 고정된 $q$에서의 DP 계산과 mask 복원에 필요한 시간과 메모리는 모두

$$
O\bigl((N-R+1)(R+1)\bigr)
$$

이다.

#### 2.3.3 최적 비율 $q$의 탐색

가능한 mask $M$에 대해

$$
I(M)-qL(M)
=L(M)\left(\frac{I(M)}{L(M)}-q\right)
$$

이고 $L(M)>0$이므로 다음이 성립한다.

$$
F_R(q)
\begin{cases}
>0,&q<q^\star,\\
=0,&q=q^\star,\\
<0,&q>q^\star.
\end{cases}
$$

최적 비율은 $F_R(q)=0$의 해이다. 이 해를 구하기 위해 Dinkelbach 방법을 사용한다.

길이 $R$의 연속 구간 중 중요도가 가장 큰 mask를 $M_C$, 중요도가 가장 높은 $R$개 행을 선택한 mask를 $M_T$라 하자.

$M_C$의 비용은 $T(R)$이고, 모든 가능한 mask는 $I(M)\le I(M_T)$, $T(R)\le L(M)$을 만족하므로 초기 하한과 상한을

$$
q_0=\frac{I(M_C)}{T(R)}
\le q^\star\le
u_0=\frac{I(M_T)}{T(R)}
$$

로 둔다.

$k$번째 반복에서 $q=q_k$로 DP를 풀고, 복원한 mask를 $M_k$라 하자. $F_R(q_k)=I(M_k)-q_kL(M_k)$을 이용하여 다음 비율을

$$
\boxed{
\begin{aligned}
q_{k+1}
&=\frac{I(M_k)}{L(M_k)}\\
&=\frac{q_kL(M_k)+F_R(q_k)}{L(M_k)}\\
&=q_k+\frac{F_R(q_k)}{L(M_k)}
\end{aligned}
}
$$

로 갱신한다.

$q_k\le q^\star$이면 $F_R(q_k)\ge0$이고, $M_k$도 정확히 $R$개 행을 선택하므로 $q_k\le q_{k+1}\le q^\star$이다. $F_R(q_k)>0$인 동안 비율은 증가하고, $F_R(q_k)=0$이면 $M_k$는 원래 비율 문제의 최적해이다. 가능한 mask의 수는 유한하므로, 정확한 DP 풀이와 산술 연산 아래에서는 유한 번의 반복으로 최적값에 도달한다.

#### 2.3.4 잔차를 이용한 상한의 갱신

임의의 최적 mask $M^\star$에 대해 $I(M^\star)=q^\star L(M^\star)$이다. 

또한 $q^\star-q_k\ge0$이고 $T(R)\le L(M^\star)$이다. 

$F_R(q_k)$의 정의에 의해 $I(M^\star)-q_kL(M^\star)\le F_R(q_k)$이므로

$$
\begin{aligned}
(q^\star-q_k)T(R)
&\le(q^\star-q_k)L(M^\star)\\
&=I(M^\star)-q_kL(M^\star)\\
&\le F_R(q_k).
\end{aligned}
$$

$T(R)>0$으로 나눈 뒤 양변에 $q_k$를 더하면

$$
q^\star
\le q_k+\frac{F_R(q_k)}{T(R)}.
$$

기존 상한 $u_k$와 위 식의 우변은 모두 $q^\star$ 이상이므로 상한을

$$
u_{k+1}
=\min\left\{
u_k,\;
q_k+\frac{F_R(q_k)}{T(R)}
\right\}
$$

로 갱신한다. 각 반복에서 $q_k\le q^\star\le u_k$를 유지하며, $q_0>0$일 때

$$
u_k-q_k\le\varepsilon q_0
$$

이면 반복을 종료한다.

구현에서는 float64 연산과 $\varepsilon=10^{-6}$을 사용한다. DP를 $K$회 호출할 때 DP 계산과 mask 복원에 드는 총 시간은 $O(K(N-R+1)(R+1))$이다.

### 2.4 타일 기반 행 선택

Tiles는 일정 길이의 타일을 단위로 행을 선택하고, 남은 예산을 하나의 연속 구간으로 채운다.

타일 길이를 양의 정수 $B$라 하자. 시작 행을 $0$과 $d=\lfloor B/2\rfloor$로 두면 두 배치의 타일은

$$
\boxed{
\begin{aligned}
\mathcal T_1&=\{C_{0,B},C_{B,B},C_{2B,B},\ldots\},\\
\mathcal T_2&=\{C_{d,B},C_{d+B,B},C_{d+2B,B},\ldots\}
\end{aligned}
}
$$

이다. 끝 행이 $N-1$을 넘는 타일은 제외하고, 각 배치에서 mask를 독립적으로 구성한다.

행 예산을

$$
R=KB+r,
\qquad
K=\left\lfloor\frac RB\right\rfloor,
\qquad
0\le r<B
$$

로 나누고, 각 배치에서 중요도 $\sum_{i\in C}v_i$가 큰 $K$개 타일을 선택한다.

$r>0$이면 선택한 타일과 겹치지 않는 길이 $r$의 연속 구간 중 중요도가 가장 큰 구간을 추가한다. $R<B$이면 중요도가 가장 큰 길이 $R$의 연속 구간을 선택한다.

예산을 채운 mask 중 $I(M)/L(M)$이 큰 것을 $M_{\mathrm{Tiles}}$로 선택한다.

$B\ge\ell_0$이고 $R\ge B$이면, two-line 모델의 추가 비용은 완전 타일과 합쳐지지 않은 잔여 구간에서만 발생할 수 있다. 따라서

$$
\alpha R
\le L(M_{\mathrm{Tiles}})
\le\alpha(R-r)+T(r).
$$

이다. $r=0$이거나 잔여 구간이 완전 타일과 합쳐지면 $L(M_{\mathrm{Tiles}})=\alpha R$이다. 한편 $B=1$이면 Tiles는 Top-$R$과 같다.

중요도 누적합과 상위 $K$개 타일의 부분 선택을 이용하기 때문에, 고정된 $B$에서 시간 복잡도는 평균 $O(N)$, 메모리 복잡도는 $O(N)$이다.


### 2.5 최소 구간 길이 제약과 최적값의 보장

Two-line 모델에서는 같은 수의 행을 선택하고 모든 구간이 충분히 길면 읽기 비용이 동일해진다. 이 성질을 이용하여 길이 제약 아래에서의 최적화 문제를 정리하고, 중요도 보존율로 원래 최적값에 얼마나 가까운지 평가해 볼 것이다. 

mask $M$의 중요도–지연시간 비율을 $q(M)=I(M)/L(M)$이라 하자.

#### 2.5.1 길이 제약 아래에서의 최적해

정확히 $R$개 행을 선택하는 문제에, 모든 최대 연속 구간 $C\in\mathcal C(M)$의 길이가 $\ell_0$ 이상이어야 한다는 조건을 추가하자. 이 조건이 가능하도록 $\lceil\ell_0\rceil\le R\le N$을 가정한다. 이때 길이 $R$의 단일 구간은 항상 조건을 만족한다.

각 구간은 $T(|C|)=\alpha|C|$를 만족하고, 구간 길이의 합은 $R$이므로 전체 읽기 비용은

$$
\begin{aligned}
L(M)
&=\sum_{C\in\mathcal C(M)}T(|C|)\\
&=\alpha\sum_{C\in\mathcal C(M)}|C|
=\alpha R.
\end{aligned}
$$

따라서 선택한 행의 위치나 구간의 수가 달라도 읽기 비용은 같다. 중요도–지연시간 비율의 분모가 고정되므로, 이 조건 아래에서는 중요도를 최대화하는 mask가 비율도 최대화한다.

그 mask를 $M_{\mathrm{sat}}^\star$, 보존 중요도를 $I_{\mathrm{sat}}=I(M_{\mathrm{sat}}^\star)$이라 하자. 길이 제약 아래에서의 최적 비율은

$$
q_{\mathrm{sat}}
=q(M_{\mathrm{sat}}^\star)
=\frac{I_{\mathrm{sat}}}{\alpha R}
$$

이다.

#### 2.5.2 일반적인 mask에 대한 보장

이제 같은 행 예산에서 길이 조건을 두지 않은 문제의 최적 비율 $q^\star$와 비교하자.

중요도가 가장 높은 $R$개 행을 선택한 Top-$R$ mask를 $M_T$, 그 중요도를 $I_T=I(M_T)$라 하자. 정확히 $R$개 행을 선택한 모든 mask는 $I(M)\le I_T$를 만족한다. 또한 two-line 모델에서 $L(M)\ge\alpha R$이므로

$$
q^\star\le\frac{I_T}{\alpha R}.
$$

이 상한은 중요도 상한 $I_T$와 비용 하한 $\alpha R$을 결합한 것이다. 두 값을 동시에 달성하는 mask가 존재하지 않더라도 성립한다.

$I_T>0$이면 $q^\star>0$이다. 정확히 $R$개 행을 선택하는 임의의 mask $M$에 대해 $q(M)\ge0$이므로

$$
\boxed{
\begin{aligned}
\frac{q(M)}{q^\star}
&\ge\frac{q(M)}{I_T/(\alpha R)}\\
&=\frac{I(M)}{I_T}\,
\frac{\alpha R}{L(M)}.
\end{aligned}
}
$$

오른쪽 마지막 식의 두 항은 각각 Top-$R$ 대비 중요도 보존율과 읽기 효율의 상한 대비 달성률이다. 후자는 행 단위 처리량 $R/L(M)$을 그 상한 $1/\alpha$로 나눈 값이다. 두 항은 모두 $0$과 $1$ 사이에 있으며, 그 곱으로 최적 비율 대비 달성률의 하한을 얻는다.

#### 2.5.3 중요도 보존율에 따른 보장

모든 최대 연속 구간의 길이가 $\ell_0$ 이상인 mask는 비용 하한 $L(M)=\alpha R$을 달성한다. 따라서 앞 절의 보장에서 읽기 효율에 해당하는 항이 $1$이 되어

$$
\frac{q(M)}{q^\star}
\ge
\frac{I(M)}{I_T}
\underbrace{\frac{\alpha R}{L(M)}}_{=1}
=\frac{I(M)}{I_T}.
$$

이 조건 아래에서는 읽기 비용을 더 줄일 수 없으므로, 중요도 보존율만으로 최적 비율 대비 달성률을 보장할 수 있다. 중요도 상한 $I_T$에 도달하지 못하더라도, 비용 쪽의 하한은 이미 달성한 상태이기 때문이다.

특히 길이 제약 아래에서의 최적 mask $M_{\mathrm{sat}}^\star$에 적용하면

$$
\boxed{
\frac{q_{\mathrm{sat}}}{q^\star}
\ge
\frac{I_{\mathrm{sat}}}{I_T}.
}
$$

$M_{\mathrm{sat}}^\star$도 원래 문제에서 선택할 수 있는 mask이므로 $q_{\mathrm{sat}}\le q^\star$이다. 따라서 길이 제약 아래에서 Top-$R$ 중요도의 $95\%$ 이상을 보존하면, 중요도–지연시간 비율도 원래 최적값의 $95\%$ 이상이다.

이 보장은 원래 최적 mask를 직접 구하지 않고도 $I_{\mathrm{sat}}$과 $I_T$만으로 확인할 수 있다. 여기서 비용 하한의 달성과 비율에 대한 보장은 two-line 모델의 추정 비용을 기준으로 한다.


## 3. 실험 내용

### 3.1 실험 01 — Chunk 크기에 따른 SSD 읽기 비용

이번 실험에서는 2.2절에서 도입한 two-line 모델이 실험 장치의 읽기 비용을 얼마나 잘 근사하는지 살펴보았다. Chunk 크기별 읽기 비용에 모델을 적합하고, 별도의 반복 측정값으로 오차를 평가하여 3.2절의 알고리즘 비교에 사용할 비용 함수를 정한다.

#### 3.1.1 측정 설정

노트북의 SSD에 저장한 128 MiB 파일을 대상으로, 6개 스레드와 O_DIRECT를 사용하여 읽기 비용을 측정하였다. 조사한 chunk 크기는 다음 두 구간의 432개이다.

- 1–320 KiB: 1 KiB 간격
- 324–768 KiB: 4 KiB 간격

한 크기의 처리량은 1.2절에서 설명한 비용 측정 방식에 따라 다음 순서로 추정하였다.

1. 같은 크기의 chunk 사이에 32 KiB를 비워 두고, chunk 수를 늘려 가며 읽는다.
2. 각 읽기 조건에서 3회 warmup 후 10회 측정하고, 그 시간의 중앙값을 사용한다.
3. 처리량의 변화가 작아지는 구간을 찾아 대표 처리량을 추정한다.

전체 크기 범위에 대한 측정은 7회 반복하였다. 각 반복에서는 크기를 방문하는 순서를 무작위로 섞어, 항상 같은 순서로 측정할 때 발생할 수 있는 영향을 줄였다.

요청 크기를 $z$ KiB, 반복 $t$에서 추정한 처리량을 $\widehat\beta_t(z)$ MiB/s라 하자. 이를 chunk당 읽기 비용으로 환산하면

$$
T_t(z)=\frac{1000z}{1024\widehat\beta_t(z)}
\qquad \mathrm{ms/chunk}
$$

이다. 이후 모델에 적합하는 값은 이처럼 배치 처리량으로 환산한 chunk당 비용이다. 처리량 계산에는 O_DIRECT 정렬을 위한 padding을 제외한 논리적 요청 크기를 사용하였다.

#### 3.1.2 모델의 적합과 평가

7회 반복 중 앞의 4회를 **train 자료**, 나머지 3회를 **test 자료**라 부른다. 모델의 계수와 전환점은 모두 train 자료만으로 결정하고, 오차는 적합에 쓰지 않은 test 자료로 평가하였다. 각 요청 크기에서 train 자료의 중앙값을 $T_{\mathrm{train}}(z)$, test 자료의 중앙값을 $T_{\mathrm{test}}(z)$라 하자.

모델 적합에는 행 수 대신 요청 크기 $z$를 사용한다. 2.2절의 $\alpha,\lambda,\ell_0$에 대응하는 KiB 단위 계수와 전환점을 각각 $\alpha_z,\lambda_z,z_0$라 하자. 가중치 한 행의 크기가 $w$ KiB이면 $z=w\ell$이므로, 행 단위 모델과는 다음 관계를 갖는다.

$$
\alpha=w\alpha_z,
\qquad
\lambda=w\lambda_z,
\qquad
\ell_0=\frac{z_0}{w}.
$$

적합한 모델은 다음 세 가지이다.

- **Two-line**: 2.2절의 모델이다. 계수 제약을 유지하면서, 측정한 각 크기를 전환점 후보로 두고 비음수 최소제곱법으로 계수를 적합하였다. 그중 train RMSE가 가장 작은 전환점을 선택하였다.
- **Hinge**: 연속된 두 직선으로 구성된 비교 모델이다. 큰 요청에서 비용이 요청 크기에 정비례해야 한다는 제약을 두지 않는다. 전환점은 two-line과 같이 train RMSE로 선택하였다.
- **Affine**: 하나의 직선으로 구성된 비교 모델이다.

두 비교 모델의 식은 다음과 같다.

$$
\begin{aligned}
T_{\mathrm H}(z)&=a_{\mathrm H}+b_{\mathrm H}z+c_{\mathrm H}(z-z_{\mathrm H})_+,\\
T_{\mathrm A}(z)&=a_{\mathrm A}+b_{\mathrm A}z.
\end{aligned}
$$

모든 크기에 같은 가중치를 부여하였으므로, 측정 간격이 촘촘한 320 KiB 이하 구간이 더 많은 관측값을 제공한다.

평가에는 두 지표를 사용한다. 적합된 모델의 예측값을 $\widehat T(z)$, 432개 요청 크기의 집합을 $\mathcal Z$라 하면, test 자료에 대한 두 지표는

$$
\mathrm{RMSE}=\sqrt{\frac{1}{|\mathcal Z|}\sum_{z\in\mathcal Z}\bigl(\widehat T(z)-T_{\mathrm{test}}(z)\bigr)^2},
\qquad
\mathrm{MAPE}=\frac{100}{|\mathcal Z|}\sum_{z\in\mathcal Z}\frac{\bigl|\widehat T(z)-T_{\mathrm{test}}(z)\bigr|}{T_{\mathrm{test}}(z)}
$$

이다. **RMSE**는 비용 예측의 절대적인 오차 크기를, **MAPE**는 측정 비용 대비 절대 오차의 평균 비율을 나타낸다. Train RMSE는 같은 식에서 $T_{\mathrm{test}}$ 대신 $T_{\mathrm{train}}$을 사용한 값이다.

#### 3.1.3 결과와 해석

그림 3.1-1은 측정한 모든 요청 크기에서의 처리량을 보여준다. 처리량은 작은 요청에서 크기와 함께 빠르게 오르다가 240 KiB 부근에서 포화한다. Train 자료에서 처리량이 가장 높은 측정 크기는 240 KiB였다. 처리량에 3점 이동 중앙값을 적용한 뒤 최고값의 99%에 처음 도달한 크기는 233 KiB였다.

<figure class="report-figure">
  <img src="figures/01/01_throughput.png" alt="요청 크기에 따른 처리량과 최고 처리량 및 99% 도달 위치">
  <figcaption><em>그림 3.1-1. 전체·train·test 자료의 읽기 비용 중앙값으로 환산한 처리량. 수직선은 train 자료의 최고 처리량 위치와, 3점 이동 중앙값을 적용한 처리량의 99% 도달 위치를 나타낸다.</em></figcaption>
</figure>

Two-line 모델의 전환점은 $z_0=208$ KiB로 선택되었다. 이 값은 전체 측정 크기에 대한 train RMSE를 최소화한 결과이며, 위의 두 처리량 지표와는 구분한다. 적합된 모델은 이 전환점에서 꺾이며, $z$를 KiB, 비용을 µs로 두면

$$
T_{\mathrm{2L}}(z)\approx
\begin{cases}
12.22+0.1251\,z, & 0<z<208,\\
0.1838\,z, & z\ge208
\end{cases}
$$

이다. 2.2절의 기호로는 $\alpha_z\approx0.1838\;\mathrm{\mu s/KiB}$, $\lambda_z\approx0.0588\;\mathrm{\mu s/KiB}$이다.

그림 3.1-2는 test 자료의 측정값과 각 모델의 예측 곡선을 비교한다. 표에는 전환점을 적합한 two-line 및 hinge 모델과 affine 모델의 오차를 정리하였다.

<figure class="report-figure">
  <img src="figures/01/02_model_fit.png" alt="Test 자료의 읽기 비용과 two-line, hinge, affine 모델의 예측 곡선">
  <figcaption><em>그림 3.1-2. 요청 크기별 읽기 비용의 측정값과 모델 예측. Test profile은 test 자료의 중앙값이다. Two-line (free)는 표의 Two-line 모델이며, Two-line (efficiency)는 전환점을 최고 처리량 크기인 240 KiB로 고정한 비교 설정이다.</em></figcaption>
</figure>

| 모델 | 전환점 (KiB) | Train RMSE (µs) | Test RMSE (µs) | Test MAPE (%) |
| --- | ---: | ---: | ---: | ---: |
| Two-line | 208 | 2.076 | 2.043 | 3.415 |
| Hinge | 233 | 1.814 | 1.731 | 2.707 |
| Affine | — | 3.887 | 3.909 | 10.467 |

Two-line 모델은 affine 모델보다 작은 test 오차를 보였으며, test RMSE는 2.043 µs, test MAPE는 3.415%였다. Train RMSE와 test RMSE도 비슷하여, 적합에 사용하지 않은 반복 측정에서도 유사한 근사 정확도를 보였다. 다만 비교 모델 중 test 오차가 가장 작은 것은 hinge 모델이었다.

크기별 오차를 더 자세히 보기 위해, 그림 3.1-3에 예측값에서 test 자료의 측정값을 뺀 잔차를 나타내었다. 예를 들어 약 480 KiB에서는 여러 모델이 실제 읽기 비용을 낮게 예측하며, 이는 그림 3.1-1에서 나타난 처리량 하락과 대응한다.

<figure class="report-figure">
  <img src="figures/01/03_residuals.png" alt="각 모델의 예측 비용에서 test 자료의 측정 비용을 뺀 잔차">
  <figcaption><em>그림 3.1-3. 요청 크기별 예측 잔차. 양수는 읽기 비용의 과대 예측, 음수는 과소 예측을 의미한다.</em></figcaption>
</figure>

이처럼 실측 처리량에는 요청 크기에 따른 국소적인 변동이 남아 있다. 2.2절에서 둔 일정한 포화 처리량은 이러한 변동을 단순화한 근사 조건이며, 이 실험에서는 그에 따른 오차를 평가하였다.

### 3.2 실험 02 — 비용 모델에서의 선택 알고리즘 비교

이번 실험에서는 3.1절에서 적합한 two-line 모델을 사용하여, Global DP, Paper, Tiles, Top-$R$의 보존 중요도와 추정 읽기 비용을 비교하였다. 2.3절의 Global DP를 고정 행 예산에서의 비율 최적해로 두고, 각 선택 방법이 이 기준에 얼마나 가까운지 살펴본다. 이 절의 지연시간은 비용 모델의 추정값이며, 행을 선택하는 계산 시간과 실제 SSD 읽기 시간은 3.3절에서 다룬다.

#### 3.2.1 입력 자료와 비용 모델

중요도는 실제 언어 모델을 실행하여 얻었다. Qwen2.5-0.5B-Instruct에 세 개의 텍스트 입력을 주고, attention과 MLP의 projection에 들어가는 activation을 수집하였다. 채널별 activation 절댓값의 평균을 그 채널의 중요도로 삼았으며, 이 중요도 벡터 하나를 trace라 부른다. 총 504개 trace를 사용하였다.

가중치는 원소당 2바이트이며, 입력 채널별 가중치를 한 행으로 배열했을 때 행 수를 $N$, 행당 원소 수를 $D$라 하자. Projection의 크기 $N\times D$는 다음 네 가지이다.

- $896\times128$
- $896\times896$
- $896\times4864$
- $4864\times896$

모든 방법에 3.1절에서 적합한 two-line 모델을 적용하였다. 한 행의 크기가 $w$ KiB이면 전환점은 $\ell_0=208/w$행이며, 예를 들어 $896\times896$에서는 $w=1.75$이므로 $\ell_0\approx118.86$이다. 선택된 행들을 최대 연속 구간으로 합친 뒤 각 구간의 비용을 더하여 $L(M)$을 계산하였다.

각 trace에서 요청 행 비율을 $f\in\{0.1,0.2,\ldots,0.9\}$로 바꾸며

$$
R=\max\{1,\lfloor fN\rfloor\}
$$

개의 행을 예산으로 주었다.

#### 3.2.2 비교 방법과 평가 지표

비교한 방법은 다음 네 가지이다.

- **Global DP**: 정확히 $R$개 행을 선택하면서 $I(M)/L(M)$을 최대화하는 2.3절의 방법이다. 다른 방법을 평가하는 기준으로 사용한다.
- **Top-$R$**: 중요도가 가장 큰 $R$개 행을 선택한다.
- **Tiles**: 2.4절의 방법이다. 타일 길이는 전환점을 올림한 $B=\lceil\ell_0\rceil$을 기본으로 하고, 이보다 길거나 짧은 길이도 함께 시험하였다.
- **Paper**: 1.2절의 greedy 방법이다. 다른 방법과 같은 two-line 비용을 제공하였다.

Global DP, Tiles, Top-$R$은 예산을 모두 채우지만, Paper는 더 적은 행을 선택할 수 있다. 비교에는 각 방법이 실제로 반환한 mask의 중요도와 비용을 사용하였다.

평가에는 두 지표를 사용한다. **보존 중요도**는 전체 중요도 중 mask가 보존한 비율이다. **DP 대비 비율**은 같은 trace와 요청 예산에서, 각 방법의 중요도–지연시간 비율을 Global DP의 값으로 나눈 것이다. 2.5절과 같이 $q(M)=I(M)/L(M)$이라 쓰고 Global DP의 mask를 $M_{\mathrm{DP}}$라 하면, 두 지표는 차례로

$$
100\cdot\frac{I(M)}{\sum_i v_i},
\qquad
100\cdot\frac{q(M)}{q(M_{\mathrm{DP}})}
$$

이며 단위는 %이다. 아래의 수치는 모두 trace 간 중앙값이며, DP 대비 비율은 trace마다 먼저 계산한 뒤 중앙값을 구하였다.

#### 3.2.3 중요도와 추정 읽기 비용

그림 3.2-1은 $896\times896$에서 요청 예산을 바꾸었을 때의 결과이다. 각 점은 하나의 trace이며, 같은 예산에서 중요도와 지연시간의 중앙값을 각각 구하여 연결하였다. 점선은 Global DP의 중앙값을 나타낸다.

<figure class="report-figure report-figure--wide">
  <img src="figures/02/01_importance_latency_896x896.png" alt="896 × 896에서 Global DP, Paper, 타일 길이별 Tiles, Top-R의 보존 중요도와 추정 읽기 비용">
  <figcaption><em>그림 3.2-1. 요청 행 비율에 따른 보존 중요도와 two-line 추정 읽기 비용. 색은 요청 비율을 나타내며, 그림의 s는 행 단위 전환점이다. Baseline/coarse와 fine 패널은 서로 다른 가로축 범위를 사용하고, 가로축은 로그 척도이다.</em></figcaption>
</figure>

요청 비율이 50%일 때는 $R=448$이다. 이 예산에서 네 방법의 결과를 정리하면 다음과 같다. Tiles는 $B=\lceil\ell_0\rceil=119$인 설정을 사용하였다.

| 방법 | 보존 중요도 (%) | 추정 읽기 비용 (ms) | DP 대비 비율 (%) |
| --- | ---: | ---: | ---: |
| Global DP | 58.44 | 0.1441 | 100.00 |
| Paper | 58.97 | 0.1483 | 96.82 |
| Tiles ($B=119$) | 56.32 | 0.1470 | 95.87 |
| Top-$R$ | 72.48 | 2.6400 | 7.22 |

Top-$R$은 중요도를 가장 많이 보존하지만, 추정 읽기 비용이 다른 세 방법의 열 배를 넘는다. Tiles와 Paper는 Top-$R$보다 중요도를 덜 보존하는 대신 읽기 비용을 Global DP와 거의 같은 수준으로 낮추었고, 두 방법 모두 최적 비율의 96% 안팎에 도달하였다.

나머지 세 shape에서도 같은 50% 예산에서 Tiles와 Paper의 DP 대비 비율은 모두 95%를 넘었고, Top-$R$은 27%에 미치지 못하였다. Tiles와 Paper를 비교하면, 네 shape 중 세 곳에서 Paper의 비율이 약 1–2%p 높았다.

<!--
작성 근거: 기존 노트북의 코드와 저장된 결과를 읽어 작성하였으며 실험은 재실행하지 않았다.
입력 수집: ../results/02/02_01_capture_traces.ipynb
비율 최적해: ../results/02/02_02_global_oracle.ipynb
선택 방법과 설정: ../results/02/02_03_selector_sweep.ipynb
그림 3.2-1: ../results/02/02_04_analyze_oracle.ipynb, cells[16].outputs[1].data[image/png]
PNG는 저장된 출력에서 그대로 추출하였다.
표와 본문의 수치: ../results/02/02_06_report_numbers.ipynb (oracle_reference.csv 및 selector_results.csv에서 계산)
보존 중요도 = 100 * importance / importance_total.
DP 대비 비율 = 100 * (importance / latency) / (DP importance / DP latency).
DP 결과는 trace_id와 fraction으로 대응시키고, shape·방법·예산별 중앙값을 구하였다.
-->

<section class="report-measurement-setup">

### 3.3 실험 03 — 실제 선택 시간과 SSD 읽기 시간 비교

이번 실험에서는 Paper, Tiles, Top-$R$을 실제로 실행하고, 선택한 행들을 SSD에 저장한 파일에서 읽는 시간을 측정하였다. 3.2절의 지연시간은 비용 모델의 추정값이었고, 행을 선택하는 계산에 걸리는 시간은 다루지 않았다. 여기서는 행을 선택하는 시간과 SSD에서 읽는 시간을 직접 측정하여 두 가지를 확인한다. 첫째는 3.2절에서 추정한 방법 간 읽기 비용의 관계가 **실제 읽기 시간**에서도 나타나는지이고, 둘째는 **행을 선택하는 계산 시간**을 포함하면 방법 간 차이가 어떻게 달라지는지이다. Global DP는 이번 실측 비교에 포함하지 않았다.

#### 3.3.1 측정 설정

3.2절의 중요도 자료에서 shape별로 저장 순서상 균등한 간격의 32개 trace를 골라, 총 128개를 사용하였다. 요청 행 비율은 $f\in\{0.3,0.4,\ldots,1.0\}$으로 바꾸었으며, 행 예산은 3.2절과 같이

$$
R=\max\{1,\lfloor fN\rfloor\}
$$

이다.

읽는 대상은 임의 바이트로 채워 SSD에 저장한 파일이다. 행마다 실제 가중치와 같은 바이트 수를 갖도록 파일의 행을 해석하고, 선택한 행 위치에 해당하는 데이터를 읽었다. 읽기에는 3.1절과 같이 최대 6개 스레드와 O_DIRECT를 사용한다. 실제 모델 가중치를 사용한 추론은 이 측정에 포함하지 않는다.

각 trace와 예산에서 3회 warmup 뒤 30회 측정하였다. 매 반복에서 방법의 실행 순서를 무작위로 섞었고, 방법마다 mask를 다시 계산한 뒤 읽기를 수행하였다.

#### 3.3.2 측정한 세 가지 시간

한 번의 반복에서 다음 세 시간을 따로 기록하였다.

| 시간 | 측정 범위 |
| --- | --- |
| **선택 시간** | 미리 준비된 중요도에서 읽을 행의 mask를 구성하는 시간 |
| **읽기 시간** | Reader가 그 mask의 행들을 읽는 시간 |
| **전체 시간** | 선택을 시작한 때부터 읽은 데이터를 반환할 때까지의 시간 |

전체 시간에는 읽기 요청과 버퍼를 준비하는 시간도 들어 있다. 따라서 전체 시간은 선택 시간과 읽기 시간의 합보다 조금 크다.

trace·예산·방법의 조합 하나를 조건이라 하자. 각 조건에서 30회 반복의 중앙값을 먼저 구하고, 같은 shape·방법·예산에 속하는 32개 trace의 중앙값으로 결과를 요약하였다.

#### 3.3.3 선택 방법의 설정

3.2절에서는 모든 방법에 two-line 모델의 비용을 주었다. 이번에는 근사 대신 3.1절에서 측정한 크기별 비용의 중앙값을 그대로 주었다. 이에 맞추어 Tiles의 기준 크기도 two-line 전환점인 208 KiB에서, 측정한 크기 중 KiB당 비용이 가장 작은 **240 KiB**로 바꾸었다.

한 행의 크기가 $w$ KiB일 때, 240 KiB에 해당하는 행 수 $s$와 기준 타일 길이 $B$는

$$
s=\frac{240}{w},
\qquad
B=\lceil s\rceil
$$

이다. 예를 들어 $896\times896$에서는 $w=1.75$이므로 $s\approx137.14$, $B=138$이다. 3.2절과 같이 이보다 길거나 짧은 타일 길이도 함께 시험하였다.

비교한 방법과 선택 계산을 수행하는 장치는 다음과 같다.

- **Paper (GPU 정렬)**: 후보 구간의 점수를 GPU에서 정렬한 뒤, 겹치지 않는 구간을 점수 순서대로 받아들인다. 선택 시간에는 점수와 정렬 결과를 GPU와 주고받는 시간이 포함된다.
- **Paper (CPU 정렬)**: 같은 점수를 CPU에서 정렬한다. GPU 정렬과 같은 mask를 반환하며, 선택 시간만 다르다.
- **Tiles**, **Top-$R$**: CPU에서 실행하는 C++ 구현이다.

Paper의 선택 시간은 후보를 얼마나 촘촘하게 만드는지에 따라 달라진다. 1.2절의 최소 후보 길이 $\ell_{\min}$과 이동 간격 상한 $J$가 작을수록 점수를 계산하고 정렬할 후보가 늘어나기 때문이다. 두 값은 측정 전에 shape마다 다음 순서로 정하였다.

1. 두 값을 각각 4–64 KiB 범위에서 바꾸어 가며, 난수 중요도에서 선택 시간(GPU 정렬)을 측정한다.
2. 선택 시간이 1.6 ms 이하인 설정 가운데 두 값의 합이 가장 작은 것을 고른다.

그 결과 두 값은 $4864\times896$에서 각각 8 KiB, 나머지 shape에서 각각 4 KiB로 정해졌다.

3.2절과 같이 Paper는 예산보다 적은 행을 선택할 수 있다. 비교는 동일한 trace와 요청 예산을 기준으로 수행하고, 중요도는 각 방법이 반환한 mask에서 계산하였다.

</section>

#### 3.3.4 보존 중요도와 종류별 실행 시간

그림 3.3-1(a)–(c)는 $896\times896$에서의 결과이다. 세로축은 모두 보존 중요도이고, 가로축은 차례로 선택 시간, 읽기 시간, 전체 시간이다. 세 그림은 같은 trace, 요청 예산과 mask를 사용한다. 각 점은 하나의 trace이며, 실선은 요청 예산별 중앙값을 연결한 것이다. 점선은 비교 기준으로 둔 Paper(GPU 정렬)의 중앙값이다.

50% 예산인 $R=448$에서의 중앙값은 다음과 같다. Tiles는 기준 길이 $B=138$을 사용하였다.

| 방법 | 보존 중요도 (%) | 선택 시간 (ms) | 읽기 시간 (ms) | 전체 시간 (ms) |
| --- | ---: | ---: | ---: | ---: |
| Paper (GPU 정렬) | 61.36 | 1.04 | 0.38 | 1.49 |
| Paper (CPU 정렬) | 61.36 | 4.25 | 0.39 | 4.71 |
| Tiles ($B=138$) | 57.74 | 0.01 | 0.41 | 0.45 |
| Top-$R$ | 74.02 | 0.03 | 1.99 | 2.05 |

**선택 시간**에서 방법 간 차이가 가장 컸다. Tiles의 선택 시간은 0.01 ms로 Paper(GPU 정렬)의 약 100분의 1이며, CPU 정렬과 비교하면 차이는 더 크다. 그림 3.3-1(a)에서 보듯이 이 차이는 다른 예산에서도 비슷하게 나타난다. 다만 100% 예산에서는 모든 방법이 선택 계산을 생략하고 전체 행을 읽는다. 그림 (a)와 (c)에서 Paper의 곡선이 마지막 점에서 왼쪽으로 이동하는 것은 이 때문이다.

<figure class="report-figure report-figure--wide">
  <img src="figures/03/01_selection_latency_896x896.png" alt="896 × 896에서 Paper, 타일 길이별 Tiles, Top-R의 보존 중요도와 행 선택 시간">
  <figcaption><em>그림 3.3-1(a). 보존 중요도와 선택 시간. 색은 요청 비율을 나타내며, 그림의 s는 240 KiB를 행 단위로 환산한 기준이다. Baseline/coarse와 fine 패널은 서로 다른 가로축 범위를 사용하고, 가로축은 로그 척도이다.</em></figcaption>
</figure>

**읽기 시간**의 방법 간 순서는 3.2절에서 비용 모델로 추정한 순서와 같았다. Tiles와 Paper의 읽기 시간은 0.4 ms 안팎으로 비슷하고, Top-$R$은 그 약 다섯 배이다. 3.2절의 추정에서는 이 차이가 열 배를 넘었으므로, 차이의 크기는 추정보다 작았다.

<figure class="report-figure report-figure--wide">
  <img src="figures/03/01_io_latency_896x896.png" alt="896 × 896에서 Paper, 타일 길이별 Tiles, Top-R의 보존 중요도와 reader 내부 읽기 시간">
  <figcaption><em>그림 3.3-1(b). 보존 중요도와 읽기 시간.</em></figcaption>
</figure>

**전체 시간**은 Tiles가 가장 짧았다. Paper는 읽기보다 선택에 더 긴 시간을 쓰므로, 읽기 시간이 비슷한데도 Tiles의 전체 시간은 Paper(GPU 정렬)의 약 3분의 1이다. Top-$R$은 선택은 빠르지만 읽기 시간이 길어, 전체 시간이 Tiles의 네 배를 넘었다. 한편 Tiles의 보존 중요도는 Paper보다 3.6%p 낮았다.

<figure class="report-figure report-figure--wide">
  <img src="figures/03/01_wall_latency_896x896.png" alt="896 × 896에서 Paper, 타일 길이별 Tiles, Top-R의 보존 중요도와 선택 및 읽기를 포함한 전체 시간">
  <figcaption><em>그림 3.3-1(c). 보존 중요도와 선택·읽기 전체 시간.</em></figcaption>
</figure>

나머지 shape에서도 같은 관계가 나타나는지 보기 위해, 같은 trace와 요청 예산끼리 짝지어 Tiles의 측정값에서 Paper의 측정값을 뺐다. Tiles는 각 shape의 기준 길이 $B=\lceil s\rceil$을, Paper는 GPU 정렬을 사용하였다. 아래 표는 shape마다 32개 trace와 8개 예산의 256쌍에서 구한 차이의 중앙값이다. 시간의 차이가 음수이면 Tiles가 더 짧다는 뜻이다.

| $N\times D$ | 보존 중요도 차이 (%p) | 선택 시간 차이 (ms) | 읽기 시간 차이 (ms) | 전체 시간 차이 (ms) |
| --- | ---: | ---: | ---: | ---: |
| $896\times128$ | +0.52 | −0.17 | +0.00 | −0.17 |
| $896\times896$ | −1.82 | −1.07 | +0.02 | −1.05 |
| $896\times4864$ | −1.11 | −1.11 | +0.06 | −1.03 |
| $4864\times896$ | −0.83 | −1.93 | +0.11 | −1.86 |

읽기 시간은 모든 shape에서 Tiles가 조금 길었으나, 그 차이는 가장 큰 $4864\times896$에서도 0.11 ms였다. 선택 시간의 차이가 이보다 훨씬 커서, 전체 시간은 모든 shape에서 Tiles가 짧았다. 보존 중요도는 $896\times128$을 제외한 세 shape에서 Tiles가 약 1–2%p 낮았다.

<!--
작성 근거: 기존 노트북, 환경 기록, 저장된 CSV와 그림을 읽어 작성하였다. 측정 실험은 재실행하지 않았으며, 종류별 시간 그림과 차이 표는 저장된 CSV로 분석 노트북을 다시 실행하였다.
Paper 튜닝: ../results/03/03_01_tune_paper.ipynb
측정: ../results/03/03_02_measure_latency.ipynb
실측 환경·튜닝 설정: ../results/03/environment.json
그림 3.3-1(a): ../results/03/03_03_compare_methods.ipynb, selection_ms-plot 셀의 두 번째 image/png 출력
그림 3.3-1(b): ../results/03/03_03_compare_methods.ipynb, io_ms-plot 셀의 두 번째 image/png 출력
그림 3.3-1(c): ../results/03/03_03_compare_methods.ipynb, wall_ms-plot 셀의 두 번째 image/png 출력
PNG는 각 노트북의 저장된 출력에서 그대로 추출하였다.
50% 예산 표의 수치: ../results/03/03_05_report_numbers.ipynb (cases.csv, timings.csv에서 계산)
시간은 case_id별로 30회 반복의 중앙값을 구한 뒤 cases.csv에 결합하였다.
50% 예산 표는 n = d = 896, fraction = 0.5에서 방법별 32개 trace의 중앙값이다.
Shape별 차이: ../results/03/03_03_compare_methods.ipynb, 4. Paired differences 출력의 Tiles (1s) 행. trace_id와 fraction을 맞춰 Tiles (1s) - Paper greedy (GPU sort)를 계산한 뒤 각 shape의 256쌍에 대한 중앙값을 구하였다. 표에는 소수 둘째 자리까지 적었다.
중요도는 cases.csv의 retained_pct를 사용하였다.
Reader의 시간 범위·분할·정렬 처리: ../vlm-flash/src/vlmflash/csrc/native.cc
Tiles 구현: ../results/03/selectors.cpp
-->

## 4. 재현 자료

작성 시점의 로컬 커밋을 기준으로 하며, 시각은 한국 표준시(KST)이다.

- [vlm-in-flash-jupyter](https://github.com/yabsed/vlm-in-flash-jupyter/): 본 보고서의 최종 실험 결과를 생성하고 재현하는 Jupyter 노트북과 실행 코드, 측정 데이터 및 분석 결과를 제공한다.

  커밋 `399315b873fa6703061dc114b4fd5bc20a5fd203` — 2026-10-28 04:00:39.

- [vlm-in-flash](https://github.com/yabsed/vlm-in-flash): 수학적 정식화와 증명, 알고리즘의 설계와 수정, 탐색적 실험과 중간 결과를 기록하여 최종 연구 방법에 이르기까지의 발전 과정을 보여준다.

  커밋 `aa904c7537541c9f89df805811d6c817454efb97` — 2026-10-28 03:58:58.


## 5. 향후 연구에서 고민해야 할 점

### 5.1 우선순위 높음

  * 원래 선행연구에서 사용한 모델은 VLM이었는데, AI 활용에 대한 부족한 지식과 구현의 용의성으로 인해 일단 Qwen의 LLM 모델로 진행했습니다. 2025년 논문에 명시된 다양한 모델을 실험해 볼 필요가 있습니다. 또한 실험 대상으로 삼은 레노버 노트북 외에도 원래 논문에서 사용된 디바이스 등에서 비교해 보는 것도 좋을 것 같습니다. 

  * 현재 실험은 노트북에 blob 파일을 만들고 해당 파일을 읽는 latency를 측정하는 것까지는 하지만, 실제로 해당 선택 알고리즘을 바탕으로 모델을 실행해 본 것은 아닙니다. 모델이 실행이 잘 되는 건지를 평가하는 방법론을 공부할 필요가 있습니다.  

### 5.2 그 외 개선할 수 있는 것

  * 지금 벤치마크 대상으로 삼고 있는 최적화 외에도, `LLM in a flash`와 같은 논문에서는 전혀 다른 관점에서 최적화를 한 것 같은데, 그러한 최적화 종류가 무엇이 있는지 탐구하고 비교해야한다고 생각합니다. 

  * VLM 모델의 작동 방식에 대한 이해도를 더 키워야 한다고 생각합니다. 

  * AI가 주피터 노트북의 코드 작성에 상당한 역할을 했고, 모든 노트북의 코드를 한번씩 읽어봤지만, 아직 tiles를 직접 구현한 C++ 코드는 읽어보지 못했습니다. 

  * 또한 주피터 노트북에서 현재 import해서 쓰고 있는 선행연구 python에 대해서도 알아볼 여지가 더 많이 있다고 생각합니다. 실험 결과에서 최적해와 비슷하게 나온 것을 보면 제대로 실행하고 있는 것 같지만, 좀 더 정확한 비교를 위해서는 서브모듈의 코드 구조에 대한 이해가 필요할 것 같습니다. 

### 5.3 AI 활용 명시

  * 이번 연구에는 AI가 강하게 이용되었습니다. 이번 주제를 선정하게 된 이유는 VLM in a Flash를 ChatGPT Astra를 이용해 수식으로 압축해서 읽다가, 2.1절에서 발견했듯 선택 가능한 Row의 갯수를 R로 고정하지 않는 이상 무조건 단일 Chunk를 고르는 것이 최적이라는 것을 AI가 알려줬고, 조건을 조금만 바꾸면 논문이 진정으로 풀고자 했던 문제에 대해서 Global DP를 통한 최적해를 구할 수 있다는 것을 ChatGPT와 함께 대화로 물어보면서였습니다. 
  
  * 논문이 기반을 두고 있는 수학적 아이디어는 대부분 ChatGPT와의 대화에서 나온 것이고, 대부분은 제가 이번 프로젝트 이전에 생각해보지 못한 것들이었습니다. Dinkelbach 방법 역시 이분탐색을 이용해서 $q$를 찾으려고 생각했던 저에게는 굉장히 놀라운 발견이었습니다. 

  * 이번 보고서의 1절은 직접 작성했고, 2절은 AI의 내용을 강하게 검토했습니다. 주피터의 코드도 모두 한번 강하게 검토했습니다. 그러나 현재 보고서의 3절은 추가적인 검토가 필요합니다. 
  
  * 특히 3절의 숫자들 중에서 주피터에서 명시적으로 계산하지 않은 숫자들은 ChatGPT가 보고서 작성 과정에서 직접 계산한 것들입니다. 해당 숫자가 정말로 나오는지 Jupyter Notebook을 이용해서 확인해야 합니다. 


