Quantize Speech Recognition Models with accuracy control using NNCF PTQ API
===========================================================================

This tutorial demonstrates how to apply ``INT8`` quantization with
accuracy control to the speech recognition model, known as
`Wav2Vec2 <https://huggingface.co/docs/transformers/model_doc/wav2vec2>`__,
using the NNCF (Neural Network Compression Framework) 8-bit quantization
with accuracy control in post-training mode (without the fine-tuning
pipeline). This notebook uses a fine-tuned
`Wav2Vec2-Base-960h <https://huggingface.co/facebook/wav2vec2-base-960h>`__
`PyTorch <https://pytorch.org/>`__ model trained on the `LibriSpeech ASR
corpus <https://www.openslr.org/12>`__. The tutorial is designed to be
extendable to custom models and datasets. It consists of the following
steps:

-  Download and prepare the Wav2Vec2 model and LibriSpeech dataset.
-  Define data loading and accuracy validation functionality.
-  Model quantization with accuracy control.
-  Compare Accuracy of original PyTorch model, OpenVINO FP16 and INT8
   models.
-  Compare performance of the original and quantized models.

The advanced quantization flow allows to apply 8-bit quantization to the
model with control of accuracy metric. This is achieved by keeping the
most impactful operations within the model in the original precision.
The flow is based on the `Basic 8-bit
quantization <https://docs.openvino.ai/2023.0/basic_quantization_flow.html>`__
and has the following differences:

-  Besides the calibration dataset, a validation dataset is required to
   compute the accuracy metric. Both datasets can refer to the same data
   in the simplest case.
-  Validation function, used to compute accuracy metric is required. It
   can be a function that is already available in the source framework
   or a custom function.
-  Since accuracy validation is run several times during the
   quantization process, quantization with accuracy control can take
   more time than the Basic 8-bit quantization flow.
-  The resulted model can provide smaller performance improvement than
   the Basic 8-bit quantization flow because some of the operations are
   kept in the original precision.

..

.. note::

   Currently, 8-bit quantization with accuracy control in NNCF is available only for models in OpenVINO representation.

The steps for the quantization with accuracy control are described below.

**Table of contents:**

- `Imports <#imports>`__
- `Prepare the Model <#prepare-the-model>`__
- `Prepare LibriSpeech Dataset <#prepare-librispeech-dataset>`__
- `Prepare calibration and validation datasets <#prepare-calibration-and-validation-datasets>`__
- `Prepare validation function <#prepare-validation-function>`__
- `Run quantization with accuracy control <#run-quantization-with-accuracy-control>`__
- `Model Usage Example <#model-usage-example>`__
- `Compare Accuracy of the Original and Quantized Models <#compare-accuracy-of-the-original-and-quantized-models>`__

.. code:: ipython3

    # !pip install -q "openvino-dev>=2023.1.0" "nncf>=2.6.0"
    !pip install -q "openvino==2023.1.0.dev20230811"
    !pip install git+https://github.com/openvinotoolkit/nncf.git@develop
    !pip install -q soundfile librosa transformers torch datasets torchmetrics


.. parsed-literal::

    Collecting git+https://github.com/openvinotoolkit/nncf.git@develop
      Cloning https://github.com/openvinotoolkit/nncf.git (to revision develop) to /tmp/pip-req-build-o2lphim0
      Running command git clone --filter=blob:none --quiet https://github.com/openvinotoolkit/nncf.git /tmp/pip-req-build-o2lphim0
      Filtering content:   1% (2/142)
      Filtering content:   2% (3/142)
      Filtering content:   3% (5/142)
      Filtering content:   4% (6/142)
      Filtering content:   5% (8/142), 10.29 MiB | 16.71 MiB/s
      Filtering content:   6% (9/142), 10.29 MiB | 16.71 MiB/s
      Filtering content:   7% (10/142), 10.29 MiB | 16.71 MiB/s
      Filtering content:   7% (10/142), 12.61 MiB | 8.69 MiB/s
      Filtering content:   8% (12/142), 12.61 MiB | 8.69 MiB/s
      Filtering content:   9% (13/142), 12.61 MiB | 8.69 MiB/s
      Filtering content:  10% (15/142), 14.35 MiB | 7.17 MiB/s
      Filtering content:  11% (16/142), 14.35 MiB | 7.17 MiB/s
      Filtering content:  12% (18/142), 14.35 MiB | 7.17 MiB/s
      Filtering content:  13% (19/142), 17.07 MiB | 6.80 MiB/s
      Filtering content:  14% (20/142), 17.07 MiB | 6.80 MiB/s
      Filtering content:  15% (22/142), 17.07 MiB | 6.80 MiB/s
      Filtering content:  16% (23/142), 17.07 MiB | 6.80 MiB/s
      Filtering content:  17% (25/142), 19.78 MiB | 6.42 MiB/s
      Filtering content:  18% (26/142), 19.78 MiB | 6.42 MiB/s
      Filtering content:  19% (27/142), 19.78 MiB | 6.42 MiB/s
      Filtering content:  20% (29/142), 19.78 MiB | 6.42 MiB/s
      Filtering content:  21% (30/142), 19.78 MiB | 6.42 MiB/s
      Filtering content:  22% (32/142), 22.80 MiB | 6.19 MiB/s
      Filtering content:  23% (33/142), 22.80 MiB | 6.19 MiB/s
      Filtering content:  24% (35/142), 22.80 MiB | 6.19 MiB/s
      Filtering content:  25% (36/142), 22.80 MiB | 6.19 MiB/s
      Filtering content:  26% (37/142), 22.80 MiB | 6.19 MiB/s
      Filtering content:  26% (37/142), 25.18 MiB | 5.93 MiB/s
      Filtering content:  27% (39/142), 25.18 MiB | 5.93 MiB/s
      Filtering content:  28% (40/142), 25.18 MiB | 5.93 MiB/s
      Filtering content:  29% (42/142), 25.18 MiB | 5.93 MiB/s
      Filtering content:  30% (43/142), 25.18 MiB | 5.93 MiB/s
      Filtering content:  31% (45/142), 25.18 MiB | 5.93 MiB/s
      Filtering content:  32% (46/142), 27.34 MiB | 5.71 MiB/s
      Filtering content:  33% (47/142), 27.34 MiB | 5.71 MiB/s
      Filtering content:  34% (49/142), 27.34 MiB | 5.71 MiB/s
      Filtering content:  35% (50/142), 27.34 MiB | 5.71 MiB/s
      Filtering content:  36% (52/142), 27.34 MiB | 5.71 MiB/s
      Filtering content:  37% (53/142), 27.34 MiB | 5.71 MiB/s
      Filtering content:  38% (54/142), 27.34 MiB | 5.71 MiB/s
      Filtering content:  39% (56/142), 27.34 MiB | 5.71 MiB/s
      Filtering content:  40% (57/142), 29.35 MiB | 5.54 MiB/s
      Filtering content:  41% (59/142), 29.35 MiB | 5.54 MiB/s
      Filtering content:  42% (60/142), 29.35 MiB | 5.54 MiB/s
      Filtering content:  43% (62/142), 29.35 MiB | 5.54 MiB/s
      Filtering content:  44% (63/142), 29.35 MiB | 5.54 MiB/s
      Filtering content:  45% (64/142), 29.35 MiB | 5.54 MiB/s
      Filtering content:  46% (66/142), 29.35 MiB | 5.54 MiB/s
      Filtering content:  47% (67/142), 29.35 MiB | 5.54 MiB/s
      Filtering content:  48% (69/142), 29.35 MiB | 5.54 MiB/s
      Filtering content:  49% (70/142), 29.35 MiB | 5.54 MiB/s
      Filtering content:  50% (71/142), 29.35 MiB | 5.54 MiB/s
      Filtering content:  51% (73/142), 29.35 MiB | 5.54 MiB/s
      Filtering content:  52% (74/142), 29.35 MiB | 5.54 MiB/s
      Filtering content:  53% (76/142), 29.35 MiB | 5.54 MiB/s
      Filtering content:  54% (77/142), 29.35 MiB | 5.54 MiB/s
      Filtering content:  55% (79/142), 29.35 MiB | 5.54 MiB/s
      Filtering content:  56% (80/142), 29.35 MiB | 5.54 MiB/s
      Filtering content:  57% (81/142), 29.35 MiB | 5.54 MiB/s
      Filtering content:  58% (83/142), 29.35 MiB | 5.54 MiB/s
      Filtering content:  59% (84/142), 29.35 MiB | 5.54 MiB/s
      Filtering content:  60% (86/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  61% (87/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  62% (89/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  63% (90/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  64% (91/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  65% (93/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  66% (94/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  67% (96/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  68% (97/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  69% (98/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  70% (100/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  71% (101/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  72% (103/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  73% (104/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  74% (106/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  75% (107/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  76% (108/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  77% (110/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  78% (111/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  79% (113/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  80% (114/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  81% (116/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  82% (117/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  83% (118/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  84% (120/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  85% (121/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  86% (123/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  87% (124/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  88% (125/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  89% (127/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  90% (128/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  91% (130/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  92% (131/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  93% (133/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  94% (134/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  95% (135/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  96% (137/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  97% (138/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  98% (140/142), 31.63 MiB | 4.19 MiB/s
      Filtering content:  99% (141/142), 31.63 MiB | 4.19 MiB/s
      Filtering content: 100% (142/142), 31.63 MiB | 4.19 MiB/s
      Filtering content: 100% (142/142), 32.00 MiB | 3.57 MiB/s, done.
      Resolved https://github.com/openvinotoolkit/nncf.git to commit 90a1e860c93b553fa9684113e02d41d622235c55
      Preparing metadata (setup.py) ... - done
    Collecting pymoo@ git+https://github.com/anyoptimization/pymoo.git@695cb26923903f872c7256a9013609769f3cc2bd (from nncf==2.5.0.dev0+90a1e860)
      Using cached pymoo-0.6.0.1-py3-none-any.whl
    Requirement already satisfied: jsonschema>=3.2.0 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from nncf==2.5.0.dev0+90a1e860) (4.19.0)
    Requirement already satisfied: jstyleson>=0.0.2 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from nncf==2.5.0.dev0+90a1e860) (0.0.2)
    Requirement already satisfied: natsort>=7.1.0 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from nncf==2.5.0.dev0+90a1e860) (8.4.0)
    Requirement already satisfied: networkx<=2.8.2,>=2.6 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from nncf==2.5.0.dev0+90a1e860) (2.8.2)
    Requirement already satisfied: ninja<1.11,>=1.10.0.post2 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from nncf==2.5.0.dev0+90a1e860) (1.10.2.4)
    Requirement already satisfied: numpy<1.25,>=1.19.1 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from nncf==2.5.0.dev0+90a1e860) (1.23.5)
    Requirement already satisfied: openvino-telemetry>=2023.1.1 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from nncf==2.5.0.dev0+90a1e860) (2023.1.1)
    Requirement already satisfied: packaging>=20.0 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from nncf==2.5.0.dev0+90a1e860) (23.1)
    Requirement already satisfied: pandas<2.1,>=1.1.5 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from nncf==2.5.0.dev0+90a1e860) (2.0.3)
    Requirement already satisfied: psutil in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from nncf==2.5.0.dev0+90a1e860) (5.9.5)
    Requirement already satisfied: pydot>=1.4.1 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from nncf==2.5.0.dev0+90a1e860) (1.4.2)
    Requirement already satisfied: pyparsing<3.0 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from nncf==2.5.0.dev0+90a1e860) (2.4.7)
    Requirement already satisfied: scikit-learn>=0.24.0 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from nncf==2.5.0.dev0+90a1e860) (1.3.0)
    Requirement already satisfied: scipy<1.11,>=1.3.2 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from nncf==2.5.0.dev0+90a1e860) (1.10.1)
    Requirement already satisfied: texttable>=1.6.3 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from nncf==2.5.0.dev0+90a1e860) (1.6.7)
    Requirement already satisfied: tqdm>=4.54.1 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from nncf==2.5.0.dev0+90a1e860) (4.66.1)
    Requirement already satisfied: attrs>=22.2.0 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from jsonschema>=3.2.0->nncf==2.5.0.dev0+90a1e860) (23.1.0)
    Requirement already satisfied: importlib-resources>=1.4.0 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from jsonschema>=3.2.0->nncf==2.5.0.dev0+90a1e860) (6.0.1)
    Requirement already satisfied: jsonschema-specifications>=2023.03.6 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from jsonschema>=3.2.0->nncf==2.5.0.dev0+90a1e860) (2023.7.1)
    Requirement already satisfied: pkgutil-resolve-name>=1.3.10 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from jsonschema>=3.2.0->nncf==2.5.0.dev0+90a1e860) (1.3.10)
    Requirement already satisfied: referencing>=0.28.4 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from jsonschema>=3.2.0->nncf==2.5.0.dev0+90a1e860) (0.30.2)
    Requirement already satisfied: rpds-py>=0.7.1 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from jsonschema>=3.2.0->nncf==2.5.0.dev0+90a1e860) (0.10.2)
    Requirement already satisfied: python-dateutil>=2.8.2 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from pandas<2.1,>=1.1.5->nncf==2.5.0.dev0+90a1e860) (2.8.2)
    Requirement already satisfied: pytz>=2020.1 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from pandas<2.1,>=1.1.5->nncf==2.5.0.dev0+90a1e860) (2023.3.post1)
    Requirement already satisfied: tzdata>=2022.1 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from pandas<2.1,>=1.1.5->nncf==2.5.0.dev0+90a1e860) (2023.3)
    Requirement already satisfied: joblib>=1.1.1 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from scikit-learn>=0.24.0->nncf==2.5.0.dev0+90a1e860) (1.3.2)
    Requirement already satisfied: threadpoolctl>=2.0.0 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from scikit-learn>=0.24.0->nncf==2.5.0.dev0+90a1e860) (3.2.0)
    Requirement already satisfied: matplotlib>=3 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from pymoo@ git+https://github.com/anyoptimization/pymoo.git@695cb26923903f872c7256a9013609769f3cc2bd->nncf==2.5.0.dev0+90a1e860) (3.5.2)
    Requirement already satisfied: autograd>=1.4 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from pymoo@ git+https://github.com/anyoptimization/pymoo.git@695cb26923903f872c7256a9013609769f3cc2bd->nncf==2.5.0.dev0+90a1e860) (1.6.2)
    Collecting cma==3.2.2 (from pymoo@ git+https://github.com/anyoptimization/pymoo.git@695cb26923903f872c7256a9013609769f3cc2bd->nncf==2.5.0.dev0+90a1e860)
      Using cached cma-3.2.2-py2.py3-none-any.whl (249 kB)
    Collecting alive-progress (from pymoo@ git+https://github.com/anyoptimization/pymoo.git@695cb26923903f872c7256a9013609769f3cc2bd->nncf==2.5.0.dev0+90a1e860)
      Obtaining dependency information for alive-progress from https://files.pythonhosted.org/packages/e3/02/5d7f9158d69b36fbe9eb0df8fb435008ec881e41bc7d839239004207d807/alive_progress-3.1.4-py3-none-any.whl.metadata
      Using cached alive_progress-3.1.4-py3-none-any.whl.metadata (68 kB)
    Requirement already satisfied: dill in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from pymoo@ git+https://github.com/anyoptimization/pymoo.git@695cb26923903f872c7256a9013609769f3cc2bd->nncf==2.5.0.dev0+90a1e860) (0.3.7)
    Collecting Deprecated (from pymoo@ git+https://github.com/anyoptimization/pymoo.git@695cb26923903f872c7256a9013609769f3cc2bd->nncf==2.5.0.dev0+90a1e860)
      Obtaining dependency information for Deprecated from https://files.pythonhosted.org/packages/20/8d/778b7d51b981a96554f29136cd59ca7880bf58094338085bcf2a979a0e6a/Deprecated-1.2.14-py2.py3-none-any.whl.metadata
      Using cached Deprecated-1.2.14-py2.py3-none-any.whl.metadata (5.4 kB)
    Requirement already satisfied: future>=0.15.2 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from autograd>=1.4->pymoo@ git+https://github.com/anyoptimization/pymoo.git@695cb26923903f872c7256a9013609769f3cc2bd->nncf==2.5.0.dev0+90a1e860) (0.18.3)
    Requirement already satisfied: zipp>=3.1.0 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from importlib-resources>=1.4.0->jsonschema>=3.2.0->nncf==2.5.0.dev0+90a1e860) (3.16.2)
    Requirement already satisfied: cycler>=0.10 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from matplotlib>=3->pymoo@ git+https://github.com/anyoptimization/pymoo.git@695cb26923903f872c7256a9013609769f3cc2bd->nncf==2.5.0.dev0+90a1e860) (0.11.0)
    Requirement already satisfied: fonttools>=4.22.0 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from matplotlib>=3->pymoo@ git+https://github.com/anyoptimization/pymoo.git@695cb26923903f872c7256a9013609769f3cc2bd->nncf==2.5.0.dev0+90a1e860) (4.42.1)
    Requirement already satisfied: kiwisolver>=1.0.1 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from matplotlib>=3->pymoo@ git+https://github.com/anyoptimization/pymoo.git@695cb26923903f872c7256a9013609769f3cc2bd->nncf==2.5.0.dev0+90a1e860) (1.4.5)
    Requirement already satisfied: pillow>=6.2.0 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from matplotlib>=3->pymoo@ git+https://github.com/anyoptimization/pymoo.git@695cb26923903f872c7256a9013609769f3cc2bd->nncf==2.5.0.dev0+90a1e860) (10.0.0)
    Requirement already satisfied: six>=1.5 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from python-dateutil>=2.8.2->pandas<2.1,>=1.1.5->nncf==2.5.0.dev0+90a1e860) (1.16.0)
    Collecting about-time==4.2.1 (from alive-progress->pymoo@ git+https://github.com/anyoptimization/pymoo.git@695cb26923903f872c7256a9013609769f3cc2bd->nncf==2.5.0.dev0+90a1e860)
      Using cached about_time-4.2.1-py3-none-any.whl (13 kB)
    Collecting grapheme==0.6.0 (from alive-progress->pymoo@ git+https://github.com/anyoptimization/pymoo.git@695cb26923903f872c7256a9013609769f3cc2bd->nncf==2.5.0.dev0+90a1e860)
      Using cached grapheme-0.6.0-py3-none-any.whl
    Requirement already satisfied: wrapt<2,>=1.10 in /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages (from Deprecated->pymoo@ git+https://github.com/anyoptimization/pymoo.git@695cb26923903f872c7256a9013609769f3cc2bd->nncf==2.5.0.dev0+90a1e860) (1.14.1)
    Using cached alive_progress-3.1.4-py3-none-any.whl (75 kB)
    Using cached Deprecated-1.2.14-py2.py3-none-any.whl (9.6 kB)
    Building wheels for collected packages: nncf
      Building wheel for nncf (setup.py) ... - \ | / done
      Created wheel for nncf: filename=nncf-2.5.0.dev0+90a1e860-py3-none-any.whl size=1139358 sha256=35a2f1daf4360a3b65a6a2996cca9f15d165f6c25994f64d8ccf10960e7a55bc
      Stored in directory: /tmp/pip-ephem-wheel-cache-mdg9hjsd/wheels/6d/17/88/a292ae87701bc65e2e1c63261d22d7fb0e15aa8448ee693d5f
    Successfully built nncf
    Installing collected packages: grapheme, Deprecated, cma, about-time, alive-progress, pymoo, nncf
      Attempting uninstall: cma
        Found existing installation: cma 2.7.0
        Uninstalling cma-2.7.0:
          Successfully uninstalled cma-2.7.0
      Attempting uninstall: pymoo
        Found existing installation: pymoo 0.5.0
        Uninstalling pymoo-0.5.0:
          Successfully uninstalled pymoo-0.5.0
      Attempting uninstall: nncf
        Found existing installation: nncf 2.5.0
        Uninstalling nncf-2.5.0:
          Successfully uninstalled nncf-2.5.0
    Successfully installed Deprecated-1.2.14 about-time-4.2.1 alive-progress-3.1.4 cma-3.2.2 grapheme-0.6.0 nncf-2.5.0.dev0+90a1e860 pymoo-0.6.0.1


Imports 
###############################################################################################################################

.. code:: ipython3

    import numpy as np
    import torch
    
    from transformers import Wav2Vec2ForCTC, Wav2Vec2Processor


.. parsed-literal::

    2023-09-08 23:07:39.211214: I tensorflow/core/util/port.cc:110] oneDNN custom operations are on. You may see slightly different numerical results due to floating-point round-off errors from different computation orders. To turn them off, set the environment variable `TF_ENABLE_ONEDNN_OPTS=0`.
    2023-09-08 23:07:39.246066: I tensorflow/core/platform/cpu_feature_guard.cc:182] This TensorFlow binary is optimized to use available CPU instructions in performance-critical operations.
    To enable the following instructions: AVX2 AVX512F AVX512_VNNI FMA, in other operations, rebuild TensorFlow with the appropriate compiler flags.
    2023-09-08 23:07:39.789011: W tensorflow/compiler/tf2tensorrt/utils/py_utils.cc:38] TF-TRT Warning: Could not find TensorRT


Prepare the Model  
###############################################################################################################################

For instantiating PyTorch model class,
we should use ``Wav2Vec2ForCTC.from_pretrained`` method with providing
model ID for downloading from HuggingFace hub. Model weights and
configuration files will be downloaded automatically in first time
usage. Keep in mind that downloading the files can take several minutes
and depends on your internet connection.

Additionally, we can create processor class which is responsible for
model specific pre- and post-processing steps.

.. code:: ipython3

    BATCH_SIZE = 1
    MAX_SEQ_LENGTH = 30480
    
    
    torch_model = Wav2Vec2ForCTC.from_pretrained("facebook/wav2vec2-base-960h", ctc_loss_reduction="mean")
    processor = Wav2Vec2Processor.from_pretrained("facebook/wav2vec2-base-960h")


.. parsed-literal::

    Some weights of Wav2Vec2ForCTC were not initialized from the model checkpoint at facebook/wav2vec2-base-960h and are newly initialized: ['wav2vec2.masked_spec_embed']
    You should probably TRAIN this model on a down-stream task to be able to use it for predictions and inference.


Convert it to the OpenVINO Intermediate Representation (OpenVINO IR)

.. code:: ipython3

    import openvino as ov
    
    
    default_input = torch.zeros([1, MAX_SEQ_LENGTH], dtype=torch.float)
    ov_model = ov.convert_model(torch_model, example_input=default_input)


.. parsed-literal::

    WARNING:tensorflow:Please fix your imports. Module tensorflow.python.training.tracking.base has been moved to tensorflow.python.trackable.base. The old module will be deleted in version 2.11.


.. parsed-literal::

    [ WARNING ]  Please fix your imports. Module %s has been moved to %s. The old module will be deleted in version %s.


.. parsed-literal::

    INFO:nncf:NNCF initialized successfully. Supported frameworks detected: torch, tensorflow, onnx, openvino
    WARNING:nncf:NNCF provides best results with torch==2.0.1, while current torch version is 1.13.1+cpu. If you encounter issues, consider switching to torch==2.0.1


.. parsed-literal::

    No CUDA runtime is found, using CUDA_HOME='/usr/local/cuda'
    /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages/transformers/models/wav2vec2/modeling_wav2vec2.py:595: TracerWarning: Converting a tensor to a Python boolean might cause the trace to be incorrect. We can't record the data flow of Python values, so this value will be treated as a constant in the future. This means that the trace might not generalize to other inputs!
      if attn_weights.size() != (bsz * self.num_heads, tgt_len, src_len):
    /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages/transformers/models/wav2vec2/modeling_wav2vec2.py:634: TracerWarning: Converting a tensor to a Python boolean might cause the trace to be incorrect. We can't record the data flow of Python values, so this value will be treated as a constant in the future. This means that the trace might not generalize to other inputs!
      if attn_output.size() != (bsz * self.num_heads, tgt_len, self.head_dim):


Prepare LibriSpeech Dataset 
###############################################################################################################################

For demonstration purposes, we will use short dummy version of
LibriSpeech dataset - ``patrickvonplaten/librispeech_asr_dummy`` to
speed up model evaluation. Model accuracy can be different from reported
in the paper. For reproducing original accuracy, use ``librispeech_asr``
dataset.

.. code:: ipython3

    from datasets import load_dataset
    
    
    dataset = load_dataset("patrickvonplaten/librispeech_asr_dummy", "clean", split="validation")
    test_sample = dataset[0]["audio"]
    
    
    # define preprocessing function for converting audio to input values for model
    def map_to_input(batch):
        preprocessed_signal = processor(batch["audio"]["array"], return_tensors="pt", padding="longest", sampling_rate=batch['audio']['sampling_rate'])
        input_values = preprocessed_signal.input_values
        batch['input_values'] = input_values
        return batch
    
    
    # apply preprocessing function to dataset and remove audio column, to save memory as we do not need it anymore
    dataset = dataset.map(map_to_input, batched=False, remove_columns=["audio"])

Prepare calibration dataset 
###############################################################################################################################

.. code:: ipython3

    import nncf
    
    
    def transform_fn(data_item):
        """
        Extract the model's input from the data item.
        The data item here is the data item that is returned from the data source per iteration.
        This function should be passed when the data item cannot be used as model's input.
        """
        return np.array(data_item["input_values"])
    
    
    calibration_dataset = nncf.Dataset(dataset, transform_fn)

Prepare validation function  
###############################################################################################################################

Define the validation function.

.. code:: ipython3

    from torchmetrics import WordErrorRate
    from tqdm.notebook import tqdm
    
    
    def validation_fn(model, dataset):
        """
        Calculate and returns a metric for the model.
        """
        wer = WordErrorRate()
        for sample in tqdm(dataset):
            # run infer function on sample
            output = model.output(0)
            logits = model(np.array(sample['input_values']))[output]
            predicted_ids = np.argmax(logits, axis=-1)
            transcription = processor.batch_decode(torch.from_numpy(predicted_ids))
            
            # update metric on sample result
            wer.update(transcription, [sample['text']])
    
        result = wer.compute()
    
        return 1 - result

Run quantization with accuracy control  
###############################################################################################################################

You should provide
the calibration dataset and the validation dataset. It can be the same
dataset. - parameter ``max_drop`` defines the accuracy drop threshold.
The quantization process stops when the degradation of accuracy metric
on the validation dataset is less than the ``max_drop``. The default
value is 0.01. NNCF will stop the quantization and report an error if
the ``max_drop`` value can’t be reached. - ``drop_type`` defines how the
accuracy drop will be calculated: ABSOLUTE (used by default) or
RELATIVE. - ``ranking_subset_size`` - size of a subset that is used to
rank layers by their contribution to the accuracy drop. Default value is
300, and the more samples it has the better ranking, potentially. Here
we use the value 25 to speed up the execution.

.. code::
   
   Execution can take tens of minutes and requires up to 10 GB of free memory

.. code:: ipython3

    from nncf.quantization.advanced_parameters import AdvancedAccuracyRestorerParameters
    from nncf.parameters import ModelType
    
    quantized_model = nncf.quantize_with_accuracy_control(
        ov_model,
        calibration_dataset=calibration_dataset,
        validation_dataset=calibration_dataset,
        validation_fn=validation_fn,
        max_drop=0.01,
        drop_type=nncf.DropType.ABSOLUTE,
        model_type=ModelType.TRANSFORMER,
        advanced_accuracy_restorer_parameters=AdvancedAccuracyRestorerParameters(
            ranking_subset_size=25
        ),
    )


.. parsed-literal::

    Statistics collection:  24%|██▍       | 73/300 [00:13<00:42,  5.37it/s]
    Applying Smooth Quant: 100%|██████████| 50/50 [00:00<00:00, 58.74it/s]


.. parsed-literal::

    INFO:nncf:36 ignored nodes was found by name in the NNCFGraph


.. parsed-literal::

    Statistics collection:  24%|██▍       | 73/300 [00:23<01:12,  3.12it/s]
    Applying Fast Bias correction: 100%|██████████| 74/74 [00:25<00:00,  2.91it/s]

.. parsed-literal::

    INFO:nncf:Validation of initial model was started


.. parsed-literal::

    INFO:nncf:Elapsed Time: 00:00:00


.. parsed-literal::

    /opt/home/k8sworker/ci-ai/cibuilds/ov-notebook/OVNotebookOps-499/.workspace/scm/ov-notebook/.venv/lib/python3.8/site-packages/torchmetrics/utilities/prints.py:62: FutureWarning: Importing `WordErrorRate` from `torchmetrics` was deprecated and will be removed in 2.0. Import `WordErrorRate` from `torchmetrics.text` instead.
      _future_warning(



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]


.. parsed-literal::

    INFO:nncf:Elapsed Time: 00:00:13
    INFO:nncf:Metric of initial model: 0.9469565153121948
    INFO:nncf:Collecting values for each data item using the initial model



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]


.. parsed-literal::

    INFO:nncf:Elapsed Time: 00:00:10
    INFO:nncf:Validation of quantized model was started
    INFO:nncf:Elapsed Time: 00:00:20



.. parsed-literal::

    0it [00:00, ?it/s]


.. parsed-literal::

    INFO:nncf:Elapsed Time: 00:00:13
    INFO:nncf:Metric of quantized model: 0.49826085567474365
    INFO:nncf:Collecting values for each data item using the quantized model



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/1 [00:00<?, ?it/s]


.. parsed-literal::

    INFO:nncf:Elapsed Time: 00:00:07
    INFO:nncf:Accuracy drop: 0.44869565963745117 (DropType.ABSOLUTE)
    INFO:nncf:Accuracy drop: 0.44869565963745117 (DropType.ABSOLUTE)
    INFO:nncf:Total number of quantized operations in the model: 94
    INFO:nncf:Number of parallel processes to rank quantized operations: 11
    INFO:nncf:ORIGINAL metric is used to rank quantizers
    INFO:nncf:Calculating ranking score for groups of quantizers



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]



.. parsed-literal::

    0it [00:00, ?it/s]


.. parsed-literal::

    INFO:nncf:Elapsed Time: 00:04:58
    INFO:nncf:Changing the scope of quantizer nodes was started
    INFO:nncf:Reverted 1 operations to the floating-point precision: 
    	__module.wav2vec2.feature_extractor.conv_layers.2.conv/aten::_convolution/Convolution_11



.. parsed-literal::

    0it [00:00, ?it/s]


.. parsed-literal::

    INFO:nncf:Accuracy drop with the new quantization scope is 0.06000000238418579 (DropType.ABSOLUTE)
    INFO:nncf:Reverted 1 operations to the floating-point precision: 
    	__module.wav2vec2.feature_extractor.conv_layers.1.conv/aten::_convolution/Convolution_10



.. parsed-literal::

    0it [00:00, ?it/s]


.. parsed-literal::

    INFO:nncf:Algorithm completed: achieved required accuracy drop 0.007826089859008789 (DropType.ABSOLUTE)
    INFO:nncf:2 out of 94 were reverted back to the floating-point precision:
    	__module.wav2vec2.feature_extractor.conv_layers.2.conv/aten::_convolution/Convolution_11
    	__module.wav2vec2.feature_extractor.conv_layers.1.conv/aten::_convolution/Convolution_10


Model Usage Example 
###############################################################################################################################

.. code:: ipython3

    import IPython.display as ipd
    
    
    ipd.Audio(test_sample["array"], rate=16000)




.. raw:: html

    
    <audio  controls="controls" >
        <source src="data:audio/wav;base64,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AwDc/9//y/+4/7L/2//5//X//P/1/wsA6f/k/9//9f8EABIAFgAyAC0AEQAEAAEAFAD6/wMAGgBLADAAQAA0ACkA///q/+r/EgBCADAAHwAcACkA+v/j//f/IQAWAAsAFgBKAE4AOwAaAA4AGQD3/97/5v8qAEgALQARACcAHAAJAPT/BwA1AE0ALQAHACwAQwAOAAAAIQAkADAAHAA1AEMAXgAsAAQACQA0ADgAJQBFAEsAVQA7ACwADgARAAAA9P/y/ycALwA0AB8AJQAqABEA5v/X/wwAJQAiACwASwBVADsADgAJAPz//P/1//L/+f8JABkAFgAEAA8AHwABAOf/1v/k/9b/zv/e//H/DgAGAPH/5v8fAB8A+v/b/+7/+v/h/9n/9P8LABQADgD3//L/7v/j/9P/xv/9/wcA5v/n/wMA+v/m//X//f/u/9v/5//m/+r/DgAZAPr/3/8AANP/wf/B/9//wP+y/8X/3//R/9b/+f8GAAQA7v/f/7v/w//L/9b/yP/f/97/0P/D/9P/1//A/7b/mv+4/7X/zv+7/9P/4//h/+T/1P/0/+b/6f/A/+z/BwAMAPr/AQAlABkA/f/p/wMA8v/x/+r/9f/8/wMAAQD6//n/BwABAN7/5//0//f/6v/3/wYAHQAvACIABwD0////AwDj//X/9P/f/8z/6f8AAAsA+v///9H/xv/M/9n/5P/j//z/6v/5/wYAFwD0//z/CwD///f/9/8UAAAADwAlADAAEgAdABkAGQASAA4ACQD6//z/9//5////GQAJAAkABgAAAOr/BAAhACEAIQAlACQABwAPAB0ACQDq//T//P/x//z/FAAHAOn/5//c/8P/xf/b/9H/3//6//3/AwAaABYA+v/j/+r/2//k//z/IQAvAAcADADx/+H/rf+n/8z/y//X/+b/AwARABkADgDp//L/DAAEAO7/9f8WACEAJQA0ADUAKQAhABIABwDy//f////v//3/LwA6ADIAKQA3ACIABAAOAPz/9//y/wsAFgAdAB0AFgD1/+T/7//m/xYADgAHAAQAFAAcAAwABgD5/9//3P/v//T//P/5/+///P/0//z/9//0//L/4f/y/+z/8v/Z/9n/0//W/+P//f8PAAYA/f/s/+H/0//X/9P/0//b/+7/BgD///L/4f/R/8v/1v/k/+P/6v/6//r/+v8MABEAAwDx/+//9/8DABkAMgAhABYAFwD9/+T/3v/c/8b/wP/X//L/+v8BAAAA9f/0//T/AwAAAAsAJwAkACkANQBAADoAOwA/ADUAHwAdACoAGQAHACUAGgAAAA8AEQADAAEA/P8JAAkA+f/u/+7/GgAkAB0ADwAAAA4ABwALAAcABwAXABcADwAhADIAIgAhAPX/7P/h/+//5v/f/+b/9/8LAOT/7//q/+//2//u/wsADwAZAAcA8f/9/wkABwD8/wYAJwAlACcAHQAUAAwACQD5//T//f8dAB0ADwAcACcAGQD9//H//f8LAPn/HwA9AE4AOAA3ACUAHwAcAA8AIQAXAC8AQABYAEoASwA/ADgAGgAZABEAGQAdADQAOABQAE0AOgAnAB0ANQAhAC0ANABVAEoATQA9AD8AUAAyADIAIQBIAEAALQAWAA4AGgAyACEAFwAlAB0AFAAMACcALwAhAAYADAASABkAEQAkADoALwAdABIADAAPAAwA+v8AAAwAEgAPAAsAAAD8/+r/+f8LABoAEQAMABIAFgAPAA8A///x/+H/7v/1//H/+f/0//H/1P/p//T/AQD0////8v/8//r/BwAOAAkAAQADAAQA//////3//P8HAA4ABwD8/+7/6v/j/+b/5v/k/wAAEgAcAAcA/P/6//f/8v/s//L/9//9/wAADgAdAAsA9f/y/97/5//q//r/7v/x/9n/0f/O/8z/0f/M/9P/zv/v/+f/7P/e/9b/y//T/97/6v8HAAEAAQD6//X/9P/n/+H///8HABIABAARABoAGgALAOf//////wEAAAAcABwAHwALAAMAAAD5//n/6v8PAB0AHQARAAwABAD3/9//0//k//z/+v/m//n/+f/3/+b/3v/q/+//8v/0/w4ACQAJAP3/9P/x/+z/3P+7/9P/1P/q/+//AwAMAAkAAwD0//T/9f8DAO//+f8MABoAHQAcABwADAD3/+z/8f/0/wQA/f/9////BwD6/wMAEQAMAAYAAAAEABIAEQASABcADAD9//r/BwAAAAAAAAD6/wAA9P/n/9z/4//k/+H/3v/c/9v/1//3//r////6/+b/6f/Z/+n/4f/p/+r/8f/y//z/9P/n/9z/2f/W/+H/8f/0//z/BgAEAPH//P8BAAMA+v///wsAJAAiABkAGQAcABcAAAADAAEADAAEAAwAHQApAA4ABgAMAA4AEQD6/wcACwAcABYAHAAaABIADAD5//3/6f///wYADgAHAAAAAwD8/+//4f/U/8z/zP/X/9v/6f/f/9//5v/k/+n/6f/3//H/9P/x//X/+v8EAO7/7//p/+P/3v/m/+n/1//b/9D/0P/J/8j/tv++/8P/1v/m/+H/6v/m/+f/4f/b/9f/3P/f/+P/9f/1/wEA9//6//X/8v/j/+P/6f/u/+f/4f/h//H/7P/f/+T/3P/j/+/////8//n/CQAJAAMAAAAGAAMAAAALAA4AFgAZABEACQAUAA4A+v/x/wMAHwAhABcAIgAcABYADAAJAAwADwAJAPn/AwAEAA8AFwAZABEAIgAdAB0AIQAUAAsABwABAAcAAQAEAOz/7v/5//f/8f/j//H/8v/y/+z/+v/9//r/AQADABEA/f///wQA/P8DAPr/+v/8/wMAAQD8/wAABAAEAAYABAAMABcAHAAWAA4AFAAMABEAGgAaABwAFwAZACIAJwAlABIAGgA3ACIAFAAZACQAKgAnADAAKQAyAC8AIQAdACIAHQAZACEAJQAhACUAHQAlACEAHQAXACIAHAAUABcABwD//wkAIQAcABoAHAAkACoALQAlACIAHAAUABkAGgAaABYAHAAdABQADwAEAAMA///5//L/9f/1//r//f/8/wsA/f8AAAsABAD9/wQADwAMAAsACQALAA4ACwAJAAYAAwD//wsAEQAUAAEACwD//wAA+v8BAPn/9f/1//X////8//z/7/8AAPf/8f////T/+f/3//r/9f///wcADAARAA4ABgAJAAwACwD///L/9//y//L/5v/m/+n/2//b/8z/3v/T/9H/3v/Z/9//3P/b/9T/5v/v////8v/q//T/5v/p/+T/5v/0/+T/4f/L/9b/y//F/8D/uP/G/7j/uP+4/7n/tv/A/8X/zP/Z/+H/4//Z/+P/3//p/+n/9P/8//f/9f/m/+7/5//x/+H/4//p/9z/5P/n/+n/5//1////BgASAAcABAAPABIADwAAAAAABAADAAwACwARABIADwAPAAkABAAAABcAFgARABQAGgAaABQAGgAfABkAFgAfABkAEgD3//T/+v/6//T/9P/0//3/9f/6/+z/8f/k/+7/6f/n/+T/7P/6////AwALAP3/BwAJAAkACwAHAAYACwABAP//9//0/wEABwABAAQAFAARABIAFAApAC8AKgAdACIALQAcACUAJwAtABoAGQAUABIADwAGAAsABwAPAAkACwAMAPn/9f8DAP3/+f/8/w8ADAAUABcAFgARAAsACwAPAAsABAAXACEAJQAhACEAHAAaAB0AIgAfACcAJQApACQAJwAhACUAOABAADsASgBKAE0ATQBSAFAATQBCAEIATgBWAEcATgBYAE4APwA7ADIALwAnACwAMgAtACcAIgAZABwAFAAfACoAJwAhACIAIQAhABkADgADAA4ACQD///3/BADm//T/9/8HAPz//f8EAAAA+v////f/+v/8//////8DAP//CQAHAAEA+f/1//n/+f/8//X/9f/1//r/9P/x//H/7v/y////BAD6//n/8v/0/wMA//8BAPr/+f/1//L////x//T/+f/6//L/6v/q/+//6v/u//f/8f/x/+r/9P/5//H//f/9//r/+v/x//L/9P8DAPT/AAADAAcAAwADAPz/AAD9//f////0//X/9f/9//f/BgALABcABwADAAcACwAEAAcACQABAAYACwARABYADgAHAAwACwAGABQACQAHAAkAAAAJAAYABAD8/////P8DAAEA9//0////8v/y////7//8/wcACwD8/wEAAwAOAA4AAQAJAAEABwD8//z/AAD9//z/BAAHAAMAAwAOAAkACwAOAAsABgADAP//AAAEAPT//f8EAA4ADwAMABIAEgAaABoAGQAMAAsACwASAAwABgAGAAkACQAGAAEA///1//L/+v/3//T/7//u/+7/+v/q//L/AQAAAPr//P8DAAMA/P8AAPn/+f/3/wAABgD8//z/AwAUABkAFwAcABwAGgAdABoAFwAkABoAGQAXABQACQARACUAIQAdAA8ACwABAAkA+f/5//r/9//3//r/AAD9/wkAEgAMABEACQAGAAsA///8//r/9f/8//n/BAD8//n/9f/3//f/5v/p/+P/5//n/+P/3//W/+T/6f/x/+7/5//p/+f/7v/q/+b/5v/f/9f/4f/X/9b/0P/J/8n/w/+4/8H/y//J/8X/w//J/8n/yf/I/8z/1P/X/9n/2f/e/9T/3v/e/97/2f/U/9z/1//Z/9z/4f/h/9v/3v/e/9z/5P/p/+7/8f/x//H/7P/x//r/9/////f/AQABAAQABAAHAAQABAADAAMAAAAEAAwADwAUABYAGQAWABwAEQAWAAwAAQAMAAYAAQABAAEA//8GAAYABAABAP//BAAJAAQA/f8GABEAFgALABEACwALAAQA+f8BAP3/+v/3//n/6f/e/9z/5P/h/97/5v/m/+f/4f/m/9n/3v/c/9n/3//e/9v/4f/m/+b/6v/p/+r/7//v//L/BgD9////AQAAAAYAAwAGABEAFwASABEACwALAAsACQADAA4AFAAUABcAHQAdAB0ANQAqADQAPwAfAD0APQA9ADUALwAkAB0ADAAHAAEABgAfABEAEQAPAAQABgD///3/CwAJABYADwAUABYACwAPAA8ADgAOAA8AFgAJAAQACQD/////CQALAAcABgAGAAcAGgARAA8AGQAiACcAJAAsACIAIgAfAB0AIgAqACUAHQApABEAFgALAAEABAAGAAMABwAMAAcADgAPABEADgAWABwABwAGAAAA/P/5//r/+f/3//T/9f/1//L/7v/p//L/AQD///n/+f/8/wAA8v/9//z/8f/u//H/8v/x//H/9f/6////8f/x//H/7P/x/+z/8v/8//H//f/5//z/9P/1////AwAAAP3/+v8AAAcABgAJAAkACQASAAYAAwD//wcAAwAAAPT/7P/q/+//6v/j/+P/3//n/+//5//m/9n/5v/p/+b/5//q/+7/7P/y//L/9//5//f/9P/y/+7/6v/k/+n/5P/m/97/3P/f/97/5//m/+T/6f/k/+T/4//h/+n/8f/5////BAADAP//AAAEAP3/9/////f//f/5//L/7P/y//z/7//y//T/AwAAAP3//P/3//X/7v/s/+T/3P/W/9P/0/++/8X/wP/J/8X/zP/D/8b/xf/O/8v/1v/M/87/1P/c/9n/2f/c/9v/1P/c/+H/3P/e/9n/1v/T/9H/0f/b/9//3v/h/+n/7P/k//f/7P/u//H/7//x//X/7//u//n/+v/0/+//7P/s/+f/5P/n/+f/5v/u//H/7v/p/+z/7//u/+n/7//0//T/9P/x//T/8v/p/+7/7P/v/+7/7v/1//f/AQD9////6f/6//n/+v/8//H/+v/6//r/+v///wcABgADAAkACwAMAAkACQAJAAMADgADABEAFwARABIAHQAXABYAIgAfAB8AFwAZABYAGgAiACUAHwAdABEAFAAkACEAFgAZABIADwAdABYAGgAUAAwAGQAZACQAGQAcABwABgAGAAYADwAHAAQAAQD8/wEA9f/0/+//8f/e/+f/9f/x/+7/6f/u/+//7//3//X/9//1//n/9f/5//L/7//y//X/7v/y/+7/7v/1//L/9f/0//r/AwADAAYA+f///wMABgADAAYAAwAGAAkABgD//wwAAQAEAAEABAADAPr/EQAGAAMAAQAMAA4AFgAXABIAGgAXACQAJwAdABkADwAdABwAHQAXABYAFAAUABEADAAdABkAEgAUABcAEgAhACoAJAAhACQAHQAkACUAIQAZABQADAAHAAsAAwD6/wEACwAPAA4AFAAOAAYABwAMAAsACwAPABIACwALAAMABwAcABkAGQAWAAsAFAAOABEAAQAMABQAFAAaABoAFgAcAC8AKgAnADAALwAyADAALQAvADAAJwAkAB8AHwAcABoALwAtAC8ALwAwACkALAAlACIAKQAqACcAKQApABoAJAAtAC8ALQAqACcAIQAZAAcABAADAPz/AAAHAA8ADwASACEAIQAdACQAIQAfACUAKgAsADAALwAqACoAKQAnAC8AOgA0ADQALAAkACEAJAAcABwAGQAcAB0AIQAlABwAGQAqACkAHAAiAB8AIgAaABkAHAARABQAEQAOAAsACQAHABwAHAAMAAYABAADAP3///8DAAMADAAMAAkABAD6//T/AQD5//L/+f/6//H/8v/s/+7/7//u//T/8v/y/+r/8f/6//n/+f/5/wAA9/////3/+f/8//r//////wAAAAAHAAMA/f/1//T/9//3/+//7v/0//r//f8AAAAA+v/1/+///f8BAPX/7v/5//3/9P/x/+7/9//5//H/6v/m/+T/8f/3//T/7//3//X/9P/k/+r/6v/m/+T/5v/f/9z/5P/j/+b/5v/m/+H/6f/j/+P/5v/v//H/+f/6//n/8v/y//T/+f/9//f/+v/1//L/7//3//X//P/u//H/8v/x/+r/8f8LAAwAEgAMAAsAAAAGAAQAAwD6//n//f/6/wMA///x//T/6v/n/9//4f/U/97/0//b/9v/4//q/+T/6f/s//f////8//X//f8BAP//AwD//wEA/f/8//n/+f8DAPz//P/9/wMAAwALAA4AFgAMAA4ADwAZABYAIgAiAC8AHAAUABQAHAAWACIAGgAaAA4ADAAJAAcAAQD9/wYACQDy//z/EQALAAMAAwD8//X/8f/0//H/8f/q/+n/5v/f/+b/9/8JAP///f////r/9//5//z////5////AQABAAMA+v8HAAwADAADAP3/9f/0//T/+f8AAAYABgAOABIAEgAPAA4AFwALAAMA//8HAP///f///wsACwAOAAQAHwAiABIAEQAXAA4ADAAOAAAA+f/8//T//f/9//z/7//y//L/6v/y//3/9P/0//H/6v/y/+r/4//q/+f/5v/p/+7/7P/j/+r/9f/3/+f/4f/e/97/2//e/97/2f/Q/9v/1v/W/9z/5v/q/+H/1P/e/+b/5//q/+//9P/6//T/+f/0//n/8v/1//r//P/0//3//P/m/+//AAD6/wEAAQD9/+f/3v/W/9z/8f///w4ADgAEAPr/AwALAAkABgD0//L/7v/5//n/8f/u/wMADgAJAO7/4f/m/9z/3v/e//L//f8OABIABgAHAAAACwAOABcAHwAPAA4ACQAPACUAJQAdAA4ABgD8//L/+v////z/+f/1/+z///8DABIAFAARAP3////8//f/+v8BAAAABAAEABEAGgAdABQABgD9//3/BAAGAA4ACQAJABEAGgARABkAKQAaABYAHwASABQABAAEAAMACwD5/wMADgAcABkAEQAXABkAEQAUABQAFgAOAAwAGgAaAAsADgAiACwAJQAdAA8AFgAJAAEA+v/0/+7/9P/3//3/+v/x/wkAAQAAAAMAAwAHAAYABwADAAYADgAXABwAKQAiACQALAAsACUAIQAdABcAGQAdABwAHwAiABoAGQAcABkAJAA4ACcAIQAZABQAFAAWAAwADwAZABQAGgAXABEACQAGABIADgADAPz/7//y/+z/7//x/+r/3P/e/9v/1//U/9T/7v/c/9n/3//W/9n/0//X/9T/2//c/+H/1//T/8j/y//I/8n/xf/D/8P/y//O/9H/1//f/9v/6v/m/+T/3//f/+b/4f/h/+b/6f/m//f/8f/3//z/7//y/+r/4//e/+P/6f/b/+T/5P/x/+z/7P/x//f/7P/v/+7/8f/v/+r/6f/5//X/7//3//f////8//z/9f/y/+r/7P/q/+P/4//m//T//P/3//T/7v/u//L/+v/y//f/9P/x/+r/6f/n/+b/5v/p/+P/5P/e/+P/5v/e/9v/3P/h/+z/5//m/97/2//e/9z/2//c/9n/3v/h/97/1v/b/9f/5P/k/+b/0//W/9n/3v/b/9n/1//e/+T/2//W/9D/2f/Z/+H/6f/m/+///f8DAAAAAwAAAAcA+v8BAP3/9f/9//3/9f/y//H/+f8EAAYABwAEAAwAFAARABoAHwAwACwANwA6AEcAOwA4AD8ARQA7ADoALwAqAC8AMgAqAC8AJAAiACUAIQAcAB0AKgAvABwAJAAlACcAJAAiAB8AHQAdAB0AEQAWAAwACQAUAAcACQAGAAsADwAJAAkABwAOAA8ACwALAAQA+v8LABIADgADAAMABgD9//L/9P/1//z//f/6/wQABgAEAAcAFgAXABYAFgAhACUAKgAvACoAKQAqAC0ALwA4ADcALQA6ADgALwAiACIAGgAdABwAIgAWABwAJwAnACQAFgALABwAFAAPAA8AAAAEAAYAAQAJAAkACQALABIADwAHAAMADAAPAAcADAAAAAwACwAPAAkABwABAP//BwD///H/8v8BAPz/AAAEAP//AwD9/wAAAwAGAAcABwALAA8ABwAMAA4ABwD1//3//////wAA+v/9//n////1//T/6f/c/+r/+f//////BAABAA4ABwALAAsABwADAAYAAQD5/+z/7v/y//n/9//x//r//f/9/wEABwAGAAQAAQD//wcA+v8HABQAEQAfABIAHQAdAB0AEgARAA4AAwABAPz/AwD8/wAADwADAAAABAAEAAQABwAEAAYABAAJAAEABgAMAAYADgAcABIACQAJAAkAAQADAP//DgAGAAkACwAGAAQAAAAAAAsAAAD6//f/AwD8/wcACQASAAwADAALAAkADwAHABIAJAAaABQAFgAPAA4ACQAAAAcA/P8BAPz/+v8AAPL/7/8OAAMA//8DAAcADwADAAkABAALAAAABwADAAwA/P8AAAMAAwD1//X/5P/e/9z/4//f/+P/1//q//H/+v/x//n/BgAAAAYABgALABEADgAEAAwADgAGAAMABAAEAPn/9//9//r/8v/0//X/9//x//n/8f/1//r/+f8EAAsABgAUABcAEgAOAAYAAQABAPr/9f/1//H/9f/u/+7/6v/f/9H/7P/c/+H/4f/q/+z/6f/x/+n/8v/y//T/9//5/+7/8f/q/+n/5P/y/+//9f/0/+7/7//v/+7/7v/n/+//6v/x/+r/8f/x/+r/9P/y//X/9//0//z/AQD1//z/9P/s//H/7//x/+n/3//b/9//5P/v/+P/9//p/+P/5v/h/9n/2//Z/9z/1v/W/9v/1//c/97/5P/n/+T/4//c/+H/yf/U/+H/3v/n/+b/3P/f/9n/3v/c/9z/4f/f/+//7v/s/+7/+v/5//X/+f/5/+//+v/0//z//f/0//z/+v/8////AAAWABcADgAMABcABwAGAAQA/f/9//z/BwADAAYA+v/5//3/9f/n/+b/6f/p/+T/5P/h/9v/5//q//T/8f/y//r/BAD1//H/5//c/9//3//b/9f/2//j/9//1//b/+T/5P/y//X/+f/5//f/AAAJAAsAGgAhACQAKgAnACIAKgAtAC8ALwAwACwALwA6AC8AJQAcACIAIgAqACkALwAnAC8AOwA9ADoALwApAB8AIgAdACEAHAAXAAsABAALAPn/AwAEAAQA//8BAAwAEQAPAA8AEgAUABcAEgARAA8AAQAEAAYABwD3//n/8f/6/wEABgAHAAwAEQAOABwAGgAfACEAKQAlACcAHQAiACIAGQAfABoAGQAaABcAFAASAAsAEgAfABQAEgARABwAJAAqADgANwA0ADQAKgApADAAGQAfAC0AGgAaACEAFwAWABYAFwAhABQAFgAWABIADgABAAMAFwAMAAsAAQALAAcABAAPAA4ACwAGAAcABwAEAPn//P8EAP//+f/3//z///8GAAYABAD///X/8f/p/9b/1P/R/+T/4//f/9z/5//s/+r/7v/k/+f/4f/f/9//3P/Q/9D/zP/O/9T/1P/R/87/2f/c/+f/5//e/97/y//M/8z/0P/W/9H/0f/U/97/6f/j/+f/5v/m//L/+f/6//////8BAAcABgAJAAQAAAD8//T/9P/x/+n/8f/8//X/+v/0//X/CwD6/wEAAAAMABEADAAUABQAEQAOAAsAAAAHAPz/+v8JAAkABAD9//z/8v/0//L/7P/u/+z/4f/q/+//5v/v//r/9P/x//H/8f/q//H/7v/0/+7/8v/0//T/9f/0//f//P/6//z/9P/v/+7/8v/5/wcADgAUABYACwAEAAYABAABAAkAAwAAAP3/AQD9////9P/1//X//f/3//n/4//j//H/7//s/+n/7P/p/+z/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2//n//L/0//G/7v/3//c/8P/u//M/9v/zv++/8X/1v/e/87/w//c/+H/yf+7/8b/7v/h/8z/yP/X/+T/0//T/9P/4f/q/+//9////w8AEQAMAAsABwALAPf/7v/5//3/+v/f/+r/5P/v/+T/3/////z/CwADAPr/DwDy/wAA//8LAA8A9/8JAAYAGgABAPX//f/6/wEA8f/q/+r/6v/f/9P/4f/s//T/4f/b/+n//P/0/+H/9P/x/+r/4f/f//X/AwAAAOf/+v8PAAkA/f///wsA////////CQAJAAAAAAD5/wkAEgAGAAwADgAPACEAFwARAAsAEgAcABYAFwARAB8AHwAhACoAEgAXAA4AKgAfAA4AJQAMABwAHQAvADIAKQAlACwANwAfABoAEgAnABoADwAUAA4AGgAJAAsABgAMAP//9P8DAP3/9//8/w8ADAD//wcABwApABwAIgAhAB0ALAAdABcADgAWABkAKQA1ACIANQA0AEMAVQBYAFUAVQBTAEMATgBIADUAKgBAAEAALQApABcAKQAqACQAFAAdADUAGgAcAA8AGQAcAB8AJAAqADQAMgAlAC0ANwAwAC0AKgA0ACUAJQARACEAJAAiABoADAAaAA8AJwAlACoANQA4AEIAQABVADcARQBYAF4AVQBHAEMAOwA1ACcAGgARAAcADgD9/wcAAQAEAAcADgAOABEAJQAdAB0AJwAcACoAJwA3ADAANQApABcAJQAXABEAAAD1/+f/7P/v/+P/7//j/+f/7P/1/+T/7v/3//H/8v/k/+n/6f/h/+f/4f/f/97/4f/R/9b/zP/J/9f/zv/U/9v/3P/h/+b/6f/b/+f/5v/p/+//9P/0//T/AADy//H/9//3////AwAGAP//EQAJAAQABgAAAAQA+f/6//r/9P/x/+b/9P/p/+7/5P/m/+7/7P/x/+7/8f/3/wMA8v/y/wEA8v/1//n/8v/u//L/+f/3//T/5v/h/+T/3//s/9//3v/j/97/4//k/+z/4f/q//H/8v/1//f/8v/6//n/7v8BAAYA+f/5//z/AwAEAP3/BgAAAAEA/f/5//X//f/q/+r/7v/q/+//6f/n/+n/6f/x//X/9//h/+r/9f/s/+P/5P/q/+//5//m//H/7//1//X/8f/5//L//f8AAAMA+v/6//z////9/wAA/f8BAAQAAAD6//r/9P/3//z/AAAGAAMAAQD//wYADAAWAA4AGgARAAwABAADAAYABgAMAAQA//8LAP3/BAADAP//AAD8//3/AQABAAMA/f8RAAcAAQADAP//DwASAA4ADwASAAsACwAPAAMABAAEAAwADgAHABQAFgAWABQAFgAMAAsADgALAAsACwABAAcAHAAUABoAFAAZABkAFAAMAA8ABwABAAMAAQD8/wAA+v8GAAQA9//y/+n/5//f/+//5v/n/+T/4//j/9T/2f/b/+f/4f/p/+T/5//m/9//3P/m/+n/6f/s/+P/5P/c/+H/2//n/9z/4//c/9//5//U/9z/0P/c/+r/+f/6//f/BgAMABQABAAAAAcA+f/6//f/+f/y/+//4//j/9v/1v/U/+z/7//s/+f/8v/v/+//6f/m//H//P/1//n/4f/b/9f/5P/c/97/3v/b/+r/6f/u/+f/7v/f/+r/1//k/+T/5//0/+f/3//U/9//2//n/9z/5v/q/+T/6v/c/+z/3v/v//X/+f/5//z/AQDy//3/8v/3/+b/7v/c/+z/5P/e/9//1P/e/9z/2//h/9n/2//k//H/7//v//r/AwD6/wAA+f8MAAEABgDv//L/6f/5/+///P/5//f/BADv/+7/4//q//f/AQD6/wEA+f8DAAcADwALABIADwAcABEAEgAMAAcAEQAMAAQADgASABcAEgAMAAwABgAJAAQADwAJAAkADwAUABcACQAEAAkABwAMABQADgAHAAEA/P/6//z/7////wsAAwDx/+//5P/f/+b/6f/u/xEACQAWAP3/AwDf//X/9/8RAA4AJAAZACcAJQAfACEAHQAfABYAFgAXAAwACwAaABcAIQAcAB0AGQAZAB8AIgAqADIALAAtAC0ALQAsAFIASwBHADcAJQAdABkAHAAiABkAIQAkAB0AHAD9////AQAGAPf/+v/1/wAA/f/9/+7/DgDy//T/4//j/9v/5v/f/+b/3//Z/9z/0f/Z/8z/1v/b/+T/4//m/9n/0f/f/+n/+v/u//T/7/8BAPz/+v/5//f/+v/y//X//f/6//f/AwAAAAAA9f///wAABAD0//X/AwAEAAQA9P8OAA8AEQAMAAYAGQAXAAwABwAOAAwADgD1//n/8v8AAOz/7v/j/wsA9P/8/+f/AwD1/wAA+v8BAAAAAAD9//z//f/s//r//f8GAP3//f/n/+//5v/5//H//P8DAAkAAAD5//T/AAAMAPr/DAALAA8AAQABAP3/BgDu/+7/9f8AAPH/5v/b//r////p/97/4f/p//z/9/8AAP3//P/8/wAADgADAA4AGQAUABYA//8GAPz/AADx//H/8v8AAP3/CQAHABEABAAXABkAHQAZABYAGQAdACoAHwAiABoAJQAdABcADwASADIAJAARABQAEgAUAAsADwAMABIAAwAaAAsADAD//xcANAAqABIADAAZAB8AHAAZACEAIQAcAB0ADgAEAAcADgAhAB0ACQAcACEALAAaACQAKQA6ADQALAA0ADgANAA3AE0APwA/AD0AQAAyADoAOwBCADUAJwA3AEAAQwAqAC0AQABVADIAPwA3AEoAMgA7ACEAPwAWACcAFAAnAAQA/f8WABcAPwADACcA7/9dAOH/RwCV/34A/P/2APf/fwDO/2wAu/8BANz/PwA1ABkAFAAGACoA3v/0/9n/+f/D/+7/8f8pAAAAIgDn/w4Ayf///9v/LADh/wcA2f8PAO7/7P/c//3/BADf//f/7/8cAPH/9P/c//3/BwDs/+f/5/8GAAkAFADy//T/7P/3/+//8f/1//r/BAAGAA8AFwABAPL/AAAMAAMA6f/1/xcAKQARAOz/6f8JAAQA5v/X//H/FgAGAPX/7v8BAAQA6v/s/wAAIgAHABwABwA/AAAAMADn/2AA9P8yANb/ZQALAAQA0f8LAJIAwP/n/zj/MAG4/2MADv6CAQ8ArgF4/agA1v9QAkr+Wf+E/+cB5/+y/nb/4ABYAYz+jf93/8wBy/7U/8v+ogFW//f/uv7rALX/0//2/jsAIgDI/5//6v+HAIr/6f+H/8YAjP8sAHr/5ADW/zIAZv+RABcAKQCH/xwAYQAtAPH/xf9lACEAPwCn/0AADwBpANT/RQAcAHEA/P84ACwAcwAqABIAFgA6ADIAAQAUABwAJwAJAAYA//8HAP3/BwD//wcA9/8UAAkABwDj////FAAcAAMACQAJABcAFAAHAPz/7P/3/wkA8v/6/+H/CwAJAAkA5v8MAAwACwDu/wMAFgAOAAEA/P8ZAAMA8v/m/wcABgAAAOH/7//h/+b/0P/c/8v/0f/L/9T/1P/B/8j/2f/f/9D/u//G/+T/0//J/7n/3P/R/9T/sv/D/8P/0f+u/6r/s/+4/6j/mv+t/6f/q/+X/6X/rv+5/7X/ov+2/8P/zv/F/87/1//f/9T/0P/b/+P/2//M/+T/5v/p/9b/3//Z/9z/yP/R/8b/1//Q/9v/0//Z/9b/3P/q/+H/9P/v/wEA+f8DAP//CQD1//n/9f/6////8f/n/9v/3//W/9n/zv/O/8P/w//D/8v/yP/U/87/0//b/9n/3v/X/9z/3P/b/9f/4//Z/9//2f/Z/9P/3P/R/9n/3v/f/+P/5v/3//X/9f/x/wAA/f/8//z/BgD6/wAA+f8OAAsABgADAAwAFAAHAAAAAwAMAAMAAQAAAAAAAwD5//L/8v/c/+//7P/u/9f/y//b/97/0//M/9f/0//X/9z/1P/U/8j/4f/X/+P/4//j/+f/5//f/9//4//q/+b/5P/j/+P/5P/k/+r/7P/s//T/AQAGAAQADgALAAwACwAZABIAFgADAAQABAARAAwAEgAPABIAFgAWABIAFAASABYAFwAcABYAHQApABoAFwAUACIAGQAfABQADgAJAAcABgAHAP//+f///xIAFAAZAB0AIgApACEAMAAtACcAJwAwACwAJAAnACwAOgAvACkAMAApACIAHQASABIACQAGAAkABwAJAAsADAAZABYAGQARABIAHAAcAA8ADwAHAA8ABwASABEABgD//wYA///1//H/9P/8////AAD3/wEA9P/0/wAABAAEAAwACwAGAAQABwAHAAcABwAGAP3/AQD0/+//9f/x//T/8v8AAPf/8v/s//L/9/8DAP3////8/wMA+v/5//r/8v/8//T/8v/u/+r/7v/p/+//7//s/+7/9P/6//T/AQDs/wEABwAJAAkABwAEAPz/AQAEAAcAAwAJAAYAFwARAAsAAQALAA8ADAAJAA4ACQAEAAcADgARAAsACQAWABEAAAABABYADgAMABQAFAAhABYAFAASABwAGgAXABkAFwAUAAwAHAAPAAAAAQD6/wEA//8BAAkABwARAAsADAAPAAQADAAUAAwAAQAEAAsAAAD6/+//7P/c/9//0f/J/8z/u//F/87/0//G/9T/0f/h/9//6v/x//L/+f8BAAMAAQABAAYAFAADAAsABAASAAkAEQAUAA8ADwAcAB0AJQAdAB0AFwAsACcAKgAnADUALAAwACkAKgAiACcAJAAqACoAIgAtADcALwAnACkAKgAwACcAJQAlACUALQAvACkALwAhACIANwA4ACwAJQAfACQAGQApACwAJwAsACcALQAsACEAJQA6AD8AOAA1ADcANwAwADAAKQApACkAJQAZAB8ABwASABYAFAAZABYAGQASABYAHwAZABkACQALAAcADwD//wQADwASAAYA/P8AAPf/+f/1//X/9f/1//r/+v/5/+T/6v/s//n/9//5/+n/6v/m/+r/7P/v//f/8f/u/+7/8f/s//T/9f/u//z/+f/8//3/9f8DAAQAAwAAAAEABwAHAAYADgAMAA8ABwAGAAYADgAOABYAHQARABwAFgAUAA8ABgASABEADwAHAP3/AAD0//n//P/5////+v/6//3/7v////3//P/3//T/9P////n/7v/1//r/+f/u/9z/4//U/9b/4f/f/97/5//u/+r/7//3//z/CQAXAB8AHQAkADAAMgAtADUAMAAtADQANwAyADIAKgAqACoAJAAfACUAHAAiADQAKgAvADAALwA9ADIAOAAyACIAIQAaABYADwAMAAQADgAHAP//9P/u/+z/7P/x////9P/6/wYABAD///f///8GAAMAAAD8//r/+f/1/wAAAQAEAAMABAD//wAA8f/x/wwACQALAAwADwAXAB8AHAAhABwAFAAOABoAFgAJABYAHwAcABQACwAMABEAFAARABwAGgAaABwAHwAaAAsAEgAiACQAIgAhAB8AHwAkAB0AIQARAB0ACQAZABcAFAAaAC0ALQAyACwAMAAqAC0ALAAkACQAIgAsACEALAAhACEAMgAkACEAGQAaABoAHAAiABoAFAAaABcAFwAXAA4ACQAWABcAEgAMAA8AAAAHAAkABwAMAAQADgAGAAYA9/8BAA4A///9////BAAEAAYA///0//X/9//3/+7/7//m/+f/7v/j/9z/2f/f/97/3v/f/8z/1P+7/7n/u/+5/7X/sP+u/7X/qv+r/7L/tv+7/8D/yP/M/8z/zP/U/9H/wP/I/8z/3//b/+P/5//W/9f/0//R/9T/0P/U/9n/1P/U/9b/4f/v/+r/9P/1//3//f8DAPn////0/+n/6v/v/+7/8f/3//X/7//y/+z/7v/u/+z/5P/f/+r/+v/8//X//f/v//z/AQD9/wQAAAD9/wYABwD///z/9f/0/+//+f/x//L/+v/8//3/9P/6/////P/1//X/+v/0/+//7P/j/+P/5P/n/+7/7v/m/+P/4f/W/9b/2//X/9T/1P/Q/97/0//e/9f/0//T/9P/zP/L/8D/xf/B/8j/y//D/8D/vf+5/7j/yf/J/9b/1v/W/+T/4f/x/+H/5//q//T/+v8JAPf/9/8AAPz//P8BAAMABwAGAA4AEQAPABEAFAASABQADwAJABQAIQAOABEAEQAMAA8ADgAJAAkAAwAPABIAEgAJABQAGQAdAB0AJQAsACkAHQAaACIAGgAdABoAHAAZAAQACQAWAAwA//8AAAYABwALAAMABwAOAA4ACQAOABwACQApADgAOwApACEALAApACEAFwAdABoAHwALAAMAAwD9/xQAIgAkABkAHAAhACQAHAAlACkAKgAtACcAHwAhAA8AFgAfABYAEgABAAcA+f/1//X/8f/5//H/9//x/+7/8v/x//3/+v/v//L/5v/c/+T/3//j/+f/5v/v//X/8f/p/+r/AAADAP//EgAAAAwAAQABAP3/BAAMAAMADwASAAQACQD//wsA+f8GAAEAAwD//////P/3/wAA/P////r/AAD9/wsADgABAP3/9f/y//T/9P/v//X/9f/s/+f/7//n/+T/5//q/+f/7P/0//z/+v/8/wAAAAAGAP//BwAAAAMA/P8GAPr/BwD//wEABwAPAAwADgASABwAEgAiACEAGQAkAC8ALQAnAC8AMAA7ADAAPQAyAC8AJQAdACUAGQAaABQAJQAcABwADgAUAA4ADwASAAYADwD//xIABwAJAO//+f8HAAMACQALABoAFgARABoAGQAZABcAFwAcACcAIQAkACQAJwAcABYAFgASAAwADAAHABkAEgAZABcAFgAPABwAJAAtACUAMgAwACwAOAA3ADQALwAqACEAIgAXABoAHwAyACQAFgAaACEAJQAkACcAIgApACUAHQAnACEAIQAcACwAIgAZAAsABwD9/wQA7v8EAPz/BAAEAAMABwD//wMAFgASAA8AEgAUAB8AHwAaABcAAAAOAAEABAAHAAEA/f8PAAwABAAGAP//+f/y//n////6//n//f/8//z/9f/x//z/7//q/+n/7//x/+b/5//m/+b/4//h/+z/7P/v//L////5//f/7P/q/9//4f/j/9//3P/Z/9f/1//O/9D/yP/L/9T/0f/e/9//4f/U/9f/1//X/9D/zP/T/9D/2f/L/9T/zP/J/8P/yf/R/9T/1v/O/9f/0//e/9b/2//Z/97/7v/m//L/5v8GANv////v/9z/CQDk/wYA4/8MAOH////Z/wQA6v/0//n/7//8/+z//f/v//f/3//1//L/5//x/+7/7P/h/+f/1P/h/9z/4//n/+//8v/s/9//3//c/9//5//x//X/6v/p/+7/2f/k/9n/3//e/+H/zP/T/8X/yP/O/9H/0f/X/9P/2f/h/+T/5v/f/9n/3P/W/9z/3v/q/+//7v/v/+r/5v/h/+P/5P/j/+b/4f/u/+7/8v/p/+r/9//0//n/9P/1//f/+v/5//z///////z/BAAEAAQADgAMABIADAASAB0AHAAlAB0AIQAiACwANwAtADIAKQAdACkAHwAUAA4ADwAJAAwADAAXABQADgAGAAkABAD1//X/DwAHAAEA/f/9/wAADAD8/wQA9f/3//f/8f/6/9//5v/0//n/8f/0//H/8v/p/+b/9f8AAAcAAwAMAA4ADgAHAA8ACwAEAAwACQAPAA4ADwAEAAEA+f/0//z/AAD9//r/CQASAAkAEgAOAAsAHQAaACkAHwAZABoAFgAUAAkAEQAaACIAGgAPAAsAAwAEAP3//f8JAA4ABwAMAAkACwAHABoAEgAPAAYABgAEABQAEgAGAAcA9P/1//L//P/1//f/BwD///f//f////3/9P/5//3/AwALAPf////9/+//7P/3//T/7P/u/+n/5//m/+T/3v/k/+r/7//j/+b/3v/e/+P/6f/e/9T/3P/c/+z/3v/f/9f/1v/T/9H/2f/R/9D/1P/Z/8z/0//e/9P/2//W/+b/7v/v/9//2f/f/9//6v/u////AwAJAAAABAAGAAwACwARABIAGgAcAA8ADgAdACUAKgAkACcAJAAyAC0ALQAnAC8AHQAdABwAHwAiACwANAAyADAAIQAsACwALQAkAB0AFgASAAwADAAPAPz//f8BAP//+v///wQAAQADAAQAAwD9//////8BAPn/8v/6/wYACQADAPf//f/0//f/+v/5//r/8f/s//3/AAABAAYADAAOAAQACQAHAAkAAwAHAAcAEgAEAAYACwABAAAA+v8OAAwAAwABAAkACwAUAAQABAD9//n/AAD//wAA9//9/wYABAABAAEA/P/y//f/AAD//wcAFgARABQADAAAAAEABAAEAAkAAwAMAAYAAAD//wAA/f8HAA8ACwABAPr/+f8MABQACwAUAAsAFwAfACQAJQAcAA4AFwAWABcAHwAZACUAKgAfABcAEgAaABwAGQAUABEAFwAlACwALAAiAB8AJQAfABkAEQASAAsA/f/8//f/9f/v/+b/5P/q/+f/5P/5//L/4//e/97/7v/q//f/7v/8//f/9P/p/+7/5v/q/+//8f/k/9v/4f/T/9v/3//f/+T/4//u/+7/7P/h/97/2//f/9v/4//s/////P/8//f/8v/q/+z/7//6////BAAMAA4A9//m/9//5P/m/wEABgALAA4ABAD6//X/5P/q//f///8HABQADAAEAPz/+v/9//3/BgAGABcAFwAUABEAEgAMAAEA+v/6/wcABwADAP//AAD3/+f/8f////f/+f8PABEADAADAAMABAD6/wsADgAaACcAHwAZAAEA+v/x/wAA/f/0//H/9P////z/9//x/+z/7//n//z/7//s////DAD8//f/9P/v//L/7//5////BAAGAAcABgAGAAAA//8BAAAABgD9/wsACQAEAP//AwAAAAMAEQAcACIAJAAnAC0AKgAXACcALQAtAC8ANwA9ADAANwAqACoAKgAnADIAQgBNAEAAPwA6ADcANAAqADAAMAAyADgAOwA/ACoALwA6ADUAMgA0ADgAOgA9AD8APwA9ADgAPwA7AEAAPQBCAEoAQgA1ADAAMAAyACIAMAAyACcAJwAqACEAJwAJABYAHwAkACUAIgAyACIALQAlACQAGQARABIAEgAUAAcACQAWABEABgAHAAYACwAPAAwACwAEAAMAAQAHAAEA///3/wMAAADy/+b/5v/e/+T/5v/q/+//7//1//n/9//0//z//f/8/+7/5//u//f/+v8HAAkADAAAAAkA+f/5//T/8f/6/wMAAwD//wAAAwD8//X/6v/q/+//7//x//z/+v/9/w4AEgARABIAEQAOAAwACwAPAAcABwD8//z/7//n/9P/5P/u/+T/7//s/+7/3P/T/8v/1//e/+z/8f/u//T/8f/y/+//5P/8/wYADgAUAB0AFAASABEAAQDu/+z/2//s//L/AQD//wEA/f/6//f/8v/0//X/DgAaABcAFgAGAPr/7//s/+P/5//v//3//f/8/+7/4//c/8j/w//M/8j/2f/n/+T/1v/U/9f/3//Z/9D/4//m/+P/5P/m/+H/1v/T/9f/1v/Z/9T/6v/u/+//7v////z///8MAAMA///6//3/AwAAAPr/9P///wAABgAGAP//8v/0//H/7P/h/97/3//6//z///8HAAYABgD/////7P/y//z//////wQA/f/y/+z/8v/j/+n/5v/n/+7/+f8BAP3/7P/6/+r/6v/k//H/DAAPABQAFgAPABIACwABAPn/BwABABIAKgAnABwAFgAPAAcA/P/9//f/AwALAAkAFgAfAA4ACwAEAP//7v/y/wAA/P/v//r/DgADAAkAAQD6/+P/6v/p/+H/2//T/+H/8v/9////AwAGAAMA/P/6/+n/6v/k/+f/5//s/97/2f/e/9f/2f/O/8v/1v/W/9z/1v/e/9b/4f/e/9n/3//m/97/4//p/9//3//Z/9f/1v/W/+H/6v/v/97/4//U/9v/2f/T/8D/zP/Q/9z/9P/y//n///8AAPH/7P/0//L/6v/9/+//FgAWABIAFAAZAA8ADAD0/+//8f/s/+z/5P/n//X/8f/y//T//f/1/wEA9/8EAP3/9f/3//L//f/x//n/+v8DAPn/CQD9//X/8v/y//n/+f8BAPn/9f/q/9n/0f/R/9H/zP/T/9z/4//y//X/9//0/97/1v/U/9z/7//1/wEAGgAGAPr/8f/1/+n/6f/u//L//f8BAAAAAwD1/+b///8GAPr/AQAGAAkAFwAUAAwABwDx/+n/+f/6////EQAhACkAKgAiAB8AIgAZABkAGgAqADQANAA3ADIAIgAfACkAHAApADAAMgAyADUALwAqAB0AFAAXABYAEQAOABcAKgAvADAANQAwACcAFwARAA4AGQAUACkALwAnAA4ACQAiAB0AHAASABcAIgAaABoADwAZABEAEgAHABYAFwAaACQALQApAB8AEgALAAQA/P8HAA4ADwAdABcAHAAMAA8ABgD8/wEACQAXAC8ANwA/AD8AJQAPAPn/9f/p/+z/+v8ZACoALQAnABoAEgD///3/AAAHABwAJAApACIAFAAUAA4AAQD8//3//P8JABIADwAGAAMA9f/s/9//3//e//L/BwAHAAcA/P/5/+//+v8AACEAKgAnABkABgDy/9T/yf/M/87/y//k/+//9f/1/+z/4//b/9z/4//n//3/+v8PAA8ACwD5/+P/1//J/9H/2//x//r/8f/u/+H/y//G/8n/yP/G/87/y//R/9f/1//O/8v/xv/b/8z/1//q/9v/9P/1/wcA8f/6//r/BgAEAAMAFAAJAAEA7//k/+n/8f/3/wkAFgAGAA4ABwAEAAMA///9/wwAEgARABIAFwAOAAsAAwDp/87/vv/G/8b/1v/u/wAAAwD5//X/9f/q/87/1//m/+n/9/8BAAEAAQDv/+T/1P/O/8v/yf/Z/+f/9//s/+n/1v/I/7X/rv+5/9v/7P/5//z/8f/e/87/zv/R/+T///8JAAkADgAMAAAA7//q//H/+v8EABEAIQAfABEADwAMAAYABAABAA8AJwAwADoAQAA6AB8AGQAPAAkAAQAJABoAKgAhABwAGQAdABEACwAJAAwAFAAXACQAIgAOADsAMAAlAA4ABgADAPz/7/8aABwAFwAXAA8AFgAZAAYADgApACQALwA0ACkALwAtACQAHwAlADQARQA9ACwAJQASABEA9f/q//H/AAAUAB8AGgAdAA8A/P/0//L/+f///xQAMgA0AC0ADwABAOr/3P/c/+b/9P8LACUAJAAJAO7/3//m/+n/7v/5/wEAEQAMAAsAFwAMAAAA+v/9////CQAMACQALwAkACEAFgASAAwAFAAMABQAHAAMABEAEQAMABEAHwAJAAQAAwAHAAQABAASABkAFgAOAAYAAQD8/97/5P8GAA4AEgAiACkAJAAiAAsAAwAJAAMA/f8AAAYABwAGAAkADwAJAAEA+f8DAAQA//8HAAsAHQAdACIADwAHAAYACwALAA4ACQAPAAYA//8HABEAFgAMAB0AEQAGAOH/5//6/wkAEgAUACUALwAtABcABwD8/wwADAAGABcADwAGAAMA+f///wYABAADAAQAAwD1//H/7P/x//3/AwD///3/CwAGAAMA+v/3/+//6v/3//f/8v/0/wMA9P/k/+H/1v/f/+P/8v///w4AFgAaABEAFgAUACQAIQAqACcAIQApADAALQAsADcALAAtADUANABFAD8AMgAtACwAJAAfACcAQwBOAFMASAA6ADoANwA6ADoANQA7AEoAQABAADsALAAiABYACQAMAB8ALAAsACIAHAASAP//+f8OAB0AFAARACQAHwAXABQAHwAZABoAEQAHAAYADAAPABIAAwD9/wcAJAAaABIAEgAfAB8AGgAdAA4ACQD///T/9P8LABoAFgAdABQAAADx/+b/5//5/wwADAAWABQACQD3/+r/1v/p//3/9P/y/+7/9//Z/9//1v/c/+b/8f/9/w4AEgD8//n/5v/c/9P/3v/u/wAABgAMAPr/5P/h/9b/zv/X/+r///8EABcADAD//+r/0//b/9f/4f/y/wAAFAAGAO7/0f/L/8v/vv+4/8X/5P/1/wYA+f/6/9//0f/B/7v/wf/B/9f/2//m/9f/1//R/8D/wP/I/8b/yf/M/9f/1//R/8X/sP+1/7X/rv+t/8X/yf/T/8n/0//O/8b/zv/Q/9H/1v/R/9H/1P/U/9f/4f/h/+P/9f/v/+f/3P/I/7n/rv+y/8X/1//n/wEADAAAAOn/0f+7/6j/o/+4/9b/5P/v/+//9f/0/+r/0//L/87/zv/U/87/3//X/8D/u//L/9T/zv/R/8n/xv/A/6L/qv/L/8H/tv+1/7n/xf/R/8b/y//e/97/6v/n//L/9P/q/9n/3P/0/w4AFAAOABQAHwAMAP//8v/6//T/7P/q/wMADAAnACQALQAwABoADAD5//r/6f/3/xIAKQAfABYABwDx//n/8v/3//X//P/6//n//P/m/+7/6v/U/9H/+v/8/wAAAwD9/+r/0P/L/+P///8UACIAOwBDAC8AEgABAPr/+f8DAAwANABWAFAANQAfAB0A/f/q/w4AGgAiACIAIgAlACQAGgAZABcAKQAyACkAPQAlAB8AJQAlAB0AGQAdABkAHwAlABoABgD0/+n/2//b/9T/3P8PAAwABwAEAP3/2f/I/8z/3P/x/xQAGQAwADgAIgD9/+T/vf+g/5v/w//0/xcAJQA0ACwAEQDp/9D/vf/M//X/HwA9AEgAUAAyABEA6f/c/8H/tv/O/+b/DAA7AE4AOwAqAP3/3//R/9z/2//8/xwANQAhABYABgD6/+n/1/+5/9b/3P/f//H/AQAGAPr/8f/m/97/4//x/+7/9/8PACUAPQA7ADAAJwAGAPL///8dACcALwAwACQAIgARAAcABwAdACoALAA4ADAAHAAcABEAJQAyAEMAPwA0AC8ALQAUACoAKQARABQAMAA1ACoAMAAcABEA/f/q/9v/9/8BABEAIQAlAB8AEQDh/8z/w//U/+H///8ZAEsATQAnAP3/9//s/9D/0f/O//3/IgAdABoAKQAdAAYA4f/n/+r/7//5/wcADAAGAO//0//X/+z/BwAXAC0ARQA6ACQA///u//3//f/5/xwAMgAqACwAHAASAPX/7//0/xYASABhAF0AYABQAD8AFAD//wAAAwAaACoASABLAE4ALQAWABEA+f8HACkAMAA1AC0ALwAcAP//5////w8AJwAsAEUAPQAwABkACwABAP//AAADAA8ANQBIAD0AFwD3//f/2//k/97//f8cABoACwD1/wcA6v/Z/9//DgAfACcAGQAUAP3/0f+w/9H/9//y/wkAEgAPABEA5v+2/6X/o//F/9H/6v8BAB8AEQD1/+H/2//Z/8z/5P8EABkAFwAWABQA///x/8j/zv/u/wMACQAMAA8AAwDu/9f/3//8//n/AwAGAAwA/P8AAPf/7/8AAAQA+v8GABoADwARAPT/6v/m/9n/0//L/97/9P/5/+7/5v/e/9v/1//W/+P/7v/u//H/9//y/9//0//U/9v/5v/c/+P/8f/6/+r/1//c/+H/3v/e/9P/yf/j//X/8f/n/wAA+f/1//L/7v/U/9n/2f/b/+n//f/0//L/+v/v/+f/8v/6//f/AAAHAAAABwAAAOb/4f/c/9b/4f/y//n/9//y/9//0f/R/9v/1//h/9H/3//x//n/2f/j/8z/7P/f//X/AwAJAAsAFAD5//H/7P/0/wAAAQAfAB0AKQAqABoABAD8//z/AAAaACQALAA6ADcALQAtABIADAAtAEUANABCAFMAUAA7ABkABgAUABQADAASAEAATgA6AEMAMAAcAAsA9f/v/xcAKgAsACkANAAhAA4ADwAGAA4AHQD///n/GQAdAPz/9f8AAAMA6v8BAA8AJwAsAAAABwAOABwAGgAfAEMATgBNAC8ALAA4ADUAFgAGABYAFgAfADgAOwAqAC0AIQAZADAAOAA4ADUALwApACUAGQABAAsAMAA4AC0AOAAsAB8AGQAJAAcAEQAUAB8AHAAGAOz/6v/v/+n/5P/s/+//8v/0//T/9f/v/8z/0P/M/8n/1//q//n/BgDx/9f/w/+7/73/uP/A/8z/6v/5/+r/2f/O/77/tv+1/8D/5/8AABIABAD8//n/9f/X/8P/1P/3//z/9/8GAPr/AAD6//f//P8JAP3/5v/k//L////8/+f/2//c/9//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KgAqABYADgAWACQAJQAkADIALAAnABwA+f8HACUANAA7AD0AQgAdABEABwADABoALQAsAEcAZQBKAEIALAAEAN7/1//j//H/BAAnAC8AMAA0AAcA6v/v/+b/7P8EAAsAKQAsABYA///q/+7/8f/x/xIAFgApADUAJQAsABcADAAOABQAOwBFAF0AVgBLADgAGQADAAYAGgAdAC0AHAAdABYA/f/0//H/8v/q//X/DAAJAAQA8f8JAPr/+f/1/+z/BwD8/wEAAADx/+r/3v/m/8z/xv/U/+b/5//j/wAABwD9//f/9f/v/+z/8v/9/wcAFAADAAEAAQD9//z/CwAEAPz/BAAJAPn/AwABAPr//P/5//X/7P/1//z/7v/8//r/+v/8/w8ADAAWAAQADAD3/+H/yP/T/9n/3v/c/+H/5P/k/+P/zv++/8n/0//X//X/AAD9/wMA5v/W/9P/yf++/8v/3P/u/wQABwD5/+n/2f/L/63/uf/f/+z/7v/y//r/7//q/+//8v8DAAwADwAPAAsA///y//L/7v/b/+b/4////wsA8v/x/+r/2f/b/+P/5v/y/wwAEgAhABIAAwD1/+f/+v8EABcAFwARAAcABwDs/+r/5//1/wEACQAdAB0AHwAHAPL/4//j/+r/6v8BACEALAApAB0ABADu/9T/0f/h/wwADgAHAAsACQAHAPL/9f/s//f/AwALABQALwApACEADAD9/w8ABAAPAC0AQwA1ABkAFAAaABcA5v8DABcAOAA6ADcAQwA0AB0AAQD//xEAHwAsADcANAAqABcAFAAEAAcADAD1/wEAGQA4AC0ALwAiACoAHwAPABYAEQAfAAYAAADv/+P/5v/k//n/CwAfAA8ADwAhABkA9//c/+f/+f8BAAYAKgAqACkADAD//+n/8f8AAB8AJQA4AEAANAAkABkAEQDy//n/DAAaACoAHQAMABIAGQAAAAAA/P8MABIAJQAyACkAIQAHAAkABAAWAP3//f8SAAMA9//y//T/8v/n/9H/1v/f/+f/8v/v//H/4//3//H/4f/c/+f/7v/1/wQACQAPAA8AAwD1//T/9//u/+z/AAAMABIACwDy/87/xf+4/67/sv/I/9v/3//c/9f/7//v/+b/zv/D/8v/wf/R/9n/7//x/+n/6f/n/+z/4f/h/+r/7v/p//T///8GAAsACQAPABcAEgASABYAHAAMAAAA/P/0/+r/8f/q/wEADgAUAAsAAQAGAP3//P/9/wcACwAXABYAJwA6AEAAOAAqAB8ADgAAAPn/9P8JABIADAARABYA/P/k/8z/2//k//r/AAAGAAMAAAD3/+z/7P/b//T///8GAA4ADwAGAPn/8v/6/wAA/f/8/wwADwASAA4A9/8BAPr/8f8AAAkADAD9//3//f/3/wMA+v/8//L/7P/1/wsAEQABAPf/9P/v/+z///8OABQAHQAaABQABgDh/9//5P/c/+n/+f8LAAwADAD6/+r/7P/n//T/AQAXAAAABgAGAAwA9/8EAAkACwAcAAwAHQAOAA8ABgAGAAEA9/8BACEAMgA0ACwAMAAtACUAHwAlACcAQwBLAEgAOwAkACIANQAyAC8AOAA0ADcANQAlADIALAA0ACoALQAfAAMAFgAWABkAAwAZAAkAAAABAAcAAQDu/+b/1//n/+f/1P/T/+H/2f/f/87/zv/b/9H/1v/b/+z/5v/q//T/9f/b/9n/1v/h/9//6v/x//H/+v/y//z/8v/9//z/9f/5//T//f8AAAsAAQD9/+n/7v/b/+T/9P////z/AAAGAAYA6f/0/wwADAD5//r/9//1/+n/5v/p/+T/3P/M/9v/3v/n/+f/7v/u//T/+f8AAP3/AQAAAOf/9f/5/wEA+f/0/+//9f/9//H/8v/1//z/DAAOABkAGgAXABYAEgAcAA4ABwADAAQAEgALACIAHAAZABEABwAGAAwACQAUABwAFAADAAEAHQASABEACwAGAAAA+f/0/+z/8f/x//f//f8JAO/////5/wAA/f/y//f/+v8DAAEA9//5//f/8v/5//n/8v/0/wcACwD//wAAAADx/+f/5v/1//f/+f/9/wwAEQAMAA8AFAAPAAsAGQAlAC0AKgAtACoAKQAkACUAJQAqADIAMgBSAEcASgBCADsAPwA6ADsAPQBLAFsAawBlAGEASwBHAFMAVgBYAGYAawBmAGMAVgBSADQAMAAvAEoATQBIAFMAYABSAEMAMAAvADUAQwA/ADsAQABCADgALQAlABcAGgA7AEgAQwBKAD8ANwAsACoAJwAlAC0ANwA4AEcAPQAvAEAANQAfAB8AMAA/ACkAKQAyAC8AOgA7AEcANAAcACkALQA4AEIANwAwADAANAA1ADIAJQApACUAJQAnAA8ABAARAA4ADAASAAsAEQARAAkADAAGAP3/8f8BAP3/8v8AABcAHQAaABYABAABAP3/+v8BABIAGQAPABYADgAEAP//BgAAAAMAFwAcAB8AJAAXAPz/AAD//xEAEQAZAAcAAAAMAAQAAQD1//H/7v/x/+H/5P/v/+z/3//h/+7/4//q//f/+f/q/+//3v/j/97/3v/k/+H/2//Q/+//5//W/97/1v/m/9v/5//q//L/5v/h/97/4f/m/+z/AQD///r/AQD9//T/9//y/+z/7//x//r/AwAOAPL/9P/8//T/5P/f/+r/5//1/wEA9//n/9//3P/h/+T/4//f/9//4f/M/9H/1P/T/9n/2//R/9P/3//e/+T/3v/W/9f/2f/e/+P/3P/x/+r/5//e/9f/xv/O/9P/2//j//H/+f/6/+n/2f/M/9//7P/x//L/+f8AAP3/8f/3/+7/7//s/+r/6v/x//z/+f/9/+z/5P/c/9b/5//v//z/BAAEAAkABAD8//H/6v/u/+z/9//9/wMABgD5//L/5//y/+f/7P/n//n//P8GAA8ADwADAPr/BAAEABYAFwAnAC0AHwAlAB8AFAABAP////8HAPz/EQARAP//AAD9/+7/+f/v//f/+v8EAAYADgALAAkABwD9//f/8f/j/9//3v/Z/9//3v/p/9z/2f/Q/9v/0//G/8n/zv/U/9n/3v/k/+r/7P/h/+f/4//X/9T/2f/W/9b/1P/M/8v/yf/R/8v/y//T/9T/3v/k/9v/0//W/9T/2f/k/+f/6f/0/+//7v/n/9z/3//f/+b/3v/j/+//7v/u/+7/5//e/9P/0//h/+H/9f/y/w4AFwAMAPr/+v/0/+b/5//j//T/9P8AAPz/8f/m/97/4f/f/97/4f/5//3/AQAMAAQA///x//T/8f/v////CQARABkACwAPAAcA/f/x//H///8LAAsABwAOAAAA7//1/+//7v/f/+T/7v/3//L////9//f/9P/6//L/9//8/wsAAwAOAAcAEQALAP//AAD5/+7/8v/n/+f/2//Z/9n/zv/Z/+H/1P/X/+b/5P/m//X/AAAAAAkAAQAEAAMA///9//f//P/1/+///P///+r/5//s/+b/9P/q//L////8/w8AHQAkABkAGgAZACUALAA4AEMASgBCADQALAAiAB8AJwApACkAHQAUACwAIQAUAP///P/9////FAAlADQANAAfACkALAAcABoAGgAZABoAGgAiACIADwAPAP3//P8SAAwADgAUAAMAAQAUABwABwAZABwAFwApAB8AHAAaABEACwAEAAYA9//6/wwAEgAXACEAKQAiAB8AGgAXABYAFwAfAB8AKQAZABQAHAAfACEAHQAPAA8AEgAMABEAAQD//wcAAAD///r/CQAMAP//8v/s/+P/6v/p/+n/8v/8//z/9f/v//H/3v/e/+n/7v/x//r/8f/3/+r/5v/n/+b/5v/e/+b/7P/q/+//8f/y/+z/5P/n/+n/8v/8/wYAAAAHAAEA/f/8//H//P/9/wMABwD8/////P/n/+n/5v/m/+f/6v/8/wAAAAD1/wAAAQD5//z//f/8//n/8v/y/+7//P/v/+r/5//h/+T/8v/x//X/8v/s//X/6f/q//X/9f/9/wcADwAMAAQAAwAJAAcAAQALAAYAAAAAAAMA+f/u/+T/3v/e/+T/5v/h/wAA///8//3/9P/0/+////8HAAAABAAAAP//7//h/9z/3P/Z/9b/1v/R/9D/1P/W/+H/6v/x//L/AQABAP//BAAGAAYA/f/5//r/AAAJAAYABAAGAP//CQAEAP3/AQAAABIADgALAAMAAQAEAAMADgAJAAkAEQASABcAFAALAAsAFAAaABYAHQAWACQAHwAiACIAHQAiABkAJAAdAAkADAAcAB0AHQAhABkAFgAUAAwAFgAWABcAMAAlACUAFAAXAB8AJAAfAB0AJAAhABwAJAAaAB0AHQAdACcAKgApACwAPwA7ADQALQAtADUAOwA4AC8AQAAqADIALwAyACQALAA0ADsAKgAiAB8AFwAcABcAEQAHAAcABAAMAAkAAQADABIADgAHAAQACQAMABQACwABAAQABwAEAP3//P/y//T/CwD///f/8v/y/+7/6f/n/+f/5//u/+//+f/6//L/+v8AAP///P/6/wYADwARABEAFgAOAAYA/f/3//n/8f/0/wMADAAGAPr/9P/y//X/+v///wcACQAJAAcADgAJABQAGgARAA4ABAALAAAA///5//T/7P/v/+//8f/5/+r/7//6//f/7//p/+r/5v/q//H/9//3//3/9P/m/+H/2//Z/+f/3//h/+7/7P/y//X//P//////BgAMAA4ADAAJAAYAFAAZABQAGQAaABEADgAJAAMABAD9//3/AAD9//L/+f8EAAkABgAWABEADgAUAAsABAABAP//9/////L/7P/k/+z/3//U/9H/1v/U/9v/4f/b/9v/3v/c/+r/5//n/+r/9//1///////x/+r/8v/0//X//f///wMABAD1//H/7//6//z/9f/y/wAAAwAGAAsADwAPAAwAEQARACIAHQAnABkAHQAUABcABwALAAcAAQAJAAAAAQDx/+z/5//h/+z/+f/8//T/9//y//r/+f/0/+n/7//x//X/+f/u/+r/8f/y//f/7v/m//f/AQADAAYADwAZABoAJAAiACcAIQAlACwANwAlACQAKQAlADQALwAsAC8ALAAqACcAIgAZABcAJQAnABYAEgAZABIADAAPAAsA+v8GAPz/BAAAAO7/6v/x/+7/5v/h/9//3P/U/9H/1v/R/9P/1v/c/97/2//f/+H/3P/c/9H/zv/I/77/vf+9/8X/xv/Q/8v/zv/Q/9H/2f/b/9v/2//m/+T/8f/s/+f/7P/m/+T/7P/x/+P/3//b/+H/1//Z/9T/1v/T/9v/2//j/+n/6v/p/+r/5v/u//r/9f/y/+n/7P/v/+r/5P/j/9z/1v/U/9//3P/M/8n/1v/W/9D/1P/T/9T/1//f/97/5P/s/+b/6v/y/+n/8f/u/+7/5v/m/9//4f/e/9b/1//J/87/0//Q/9P/3P/c/9f/2f/W/9n/3v/k/+z/6v/j/+n/3//n/9//5P/b/9f/3v/f/9b/1v/T/87/w//D/8n/1v/Q/87/zP/O/8z/0P/Z/9H/2f/U/97/3//h/97/3//m/+7/9f/s//T/7P/0/wcADgAGAAkAEQALABoAEgAaAB0AIgAnACIAGgASAA4AGgAhAB8AJQAlACUAHQAlACoALAA1ACoAMgAtACQAJwA4ADIAJQAfAC0AMAA0ADAALAAlACQAKgAsACUAIQAZACcALQAsACwALQAkACEAIQAlACwAKgAhAB8AJAAcACIAMgAqACQAHQAhACUAJwAdACIAGQAdABoAHAAfABoAJQA6ADoANwA0AC8AMAAyACoAKgAkACEAHQAhACcAGQAcACUAJwAfABcAIQAXABoAFgAUABoAHAAXABEAFwAPABkAJAAaAA8ADAADAAAAEQAMAAwABAAPAAwAGQASAAcADwApACQAIQAkACIAHQAfAB8AHwAcABwAHwAfACEAGQARACUAGQAXABQADgADAAEA9//6//X/5v/k/+H/3v/R/9//9//m/97/3P/f/9z/4//e/9//4f/k/+//6v/q/+P/5P/p//X/6v/q/+r/6v/s/+7/9//9//3/CQAOABIAFAAaAB0AHwAXABoAHAAfAB0AKQApACcAJwAsACwAJwAWABYAJAAhABoAHQAcABwAHAAcAB0AHQAlAB0AEQARAPn/AQARAAsACQAGAAkAAwAPABEAAAAJAPr////0//H/5//f/+b/4f/b/9v/4f/c/+H/5P/j/+T/5//q/+b/5//n//X/9//9//T/9P/9//r/AQAAAPz////6/wAA+f/9//f//P8BAP//AAAGAAAA/P/5//X///8GAAkADgASAAwAFwAWABwAFgAXAAkACwAPAAkAGQASABYADAAPAA8AEQAEAAAAGgAUAB0AHAAdABoAHAAnACcAIQAdABoAHQAWAAQABgALAP/////5//n//f8DAAQACQAJAA8AEgAOABcADwAXACQAJQAnACUAJAApACkAJwAaAB8AIgAXABEADgD9/wYAEQAMAAEA/f/9////BgAGAAMABAAAAAAA+v////T/9P8HAAwAAAAEAAAA9//1/+//6v/k/9//1//X/9P/zv/R/9b/zP/J/8X/wf/M/8n/zP/R/87/0f/e/9z/3//j/9z/6v/y/+r/7v/p/+r/5//c/97/3P/n/+T/5v/k/+P/4f/X/9//5//h//H/+v/x/+r/5//p/+T/5P/h/97/1P/X/9b/4//T/87/0//I/8b/vv/F/8P/vf/J/87/0P/Z/9f/5P/m/9//3P/f/+P/5//s/+z/6f/s/+z/5v/q/+f/5//q/9z/3P/c/9z/3//Z/97/4f/u/+r/7v/y//H/9f/3/wcABgADAPL/8f/v/+f/8v/e/+P/7v/p/+P/5//R/8j/1v/e/9//1v/e/+H/4f/f/+z/4f/x/+P/6f/h/9//3P/f/9v/2f/X/9//3P/h/97/3v/f/+f/4//j/9//2f/X/+z/8f/q/+T/5v/v/+b/9P/q/+7/9P/3/+z/7//y//L//f/6/wQABwAOABEAGQAcABkAJAApAB0AJwAcABYAHAAnADQAKgAlACcALwAwAC8ANAA6ADUAOgA6AD0AKQAvAEAAOwA9ADoANQA1ADQALQAkACIAHQAiACIAIgALAAkAFAAPABIAHAAnACoAJAApACUAMgAwAC8ANQA3AC0ALQA9ADUAJAAiAB8AJAAWABkAFgASABkADwAWAA8ABgAHACEAJwAdACEAJQAiACIAHAAfAA4AEQAWABwAHQAHAAcADAAGAPT/9//0//r/7//0//z//P8BAPr///8HAAYAFAAcABwAGQALABEAFgAMAAsABgAMABYABwARABEAAQD8/xEADgAJAAEADgAHAAYABAAMAAcA9//8/wEAAQDv/+z/DgD9//H/6v/1//H/+v/1//f//P/x//H/9P/x/+P/3//k/9//wP/D/9b/3//T/8n/2//f/9T/2//q/+f/3P/k/+7/6f/e/9//7//s/+P/7//8////+v/3//T/AQDv//T/BwABAAYA//8LAAwAEQASABQAGgAZABkAJwAkAAAABgAWACcABgARABoADwAOAA8ADAAHAPn/9f8AAO//1P/X//H/2//R/9//3//X/9P/yf/J/8z/wP/M/8v/xf+5/8j/y//Q/8b/0//b/9f/0//Z/9T/1//Z/9f/5v/p/9z/7P/q/+z/5//y//3////q//L////6/+//7P/x//f/5//q//r//P/u//T/+v/6/+//6v/1//3/9f/y//H/BAD6//L/BgADAAAAAwABAAsABwAJAAsABgD0/+//7//q/9v/5/8HAAYA9/8DABIAFAAOABIACwAZACcAHAAsAD0AJwAsADAAMgAfACEALAA4ACcADAAcADAALADp/yEAMgALABkANwA0ACQAHQAnACUAHAAEABQAGQAGAAkAAQD9/97/3P/s/+f/2//v/+H/3P/Z/9b/2//Z/9v/0P/h/+P/zP/c/+f/5v/O/8z/0P/T/7j/rf/A/8P/uP+5/8D/wP++/7X/0f/O/9P/6f/1//X/4//x/+7/5v/u/+H/4f/s/9v/3v/8/+7/5v/h/+7/4f/Z/9P/2//n////6f///wQA/f8AABYAEgAGABkAFAAaAAkA/P8aAPf//P/y//n/8f/f/9z/DADu/+f//f///wQABwD9/w8AFwD9/xcAJQAPAAcAFgD//w8A///6//X/////////7v8DAAQACwAUACUA5/8BADoAJQAAAAAAFwAvACEACwAZAA8AEgD//xYAEgDy/9H/KQASAOn/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zP+b/4L/ff+1//z/RwCGAKgAugB7AC0A3/+l/53/q//T/wAALwA3AC0AEQDe/7X/o/++/87/9P8dABwACwD//9v/vv+r/6j/uf/X/wkAIgBIAEoAOgAcAP3/8f/f/9n/0P/M/9H/3//0//3/BwARABYAFgD9//H/9f/8/wQACQAkADUAQgBCADcAFwAAAOP/yf/L/9T/1v/k//f/CQAkADUAOAAqABYA6f/B/6v/mP+g/7b/wP/W/+T/+f/6//X/7P/x//3/BwAWABEADAD3/9//1//G/7j/rv+w/7P/wf/O/9//5P/b/9f/2//m/+////8SAA8A/f/u/8P/rv+n/6X/uP/M/+r/FgAqAC0AIgAAAOP/xf+z/7D/zv/Q/9v/4//u//r/9P/5/wAA9P/x/wYADgABAPr/7//X/9H/zv/Q/9b/1P/m//H//f/0/+H/zP+9/6v/n/+u/8X/2//y/+r/7//v/9z/zv/R/9D/1v/h//3/CQARAAkA/f/h/8X/uP+7/7P/1v/b//L/BwD//wEA/P8GAPf/9//n/+7/4f/c/9f/1//Z/+T/8v8JAA8ADgAWAB8AIgAlACEADAAGAAEABwAGAPr/8f/p/97/1v/j/wcAHAAqACkALAAsACkADgD5////BgAdADoAWABgAGMAWABFACoAEgAGABEAFAAtAD0ATQBTAEgAPQAqAB0AHwBHAGMAZQCBAIcAhAB8AFYAPwAwADcAQABKAFAASgBhAFgANwARAPf/7//q//r///8dAD0APwA/ACoAKQAJAA4AEgAZABYADAAAANz/zP+r/6P/qv+9/+P/AQApACoAKQAXAPT/2f+w/7X/w//W/+//BwAPAA8A9f/u//T/7//1/w4AFAApADgAPwBLAEsANwAsACEADAAUAA8AEQADAP3/DwAcACIAKQAlAC8AMAAsADAAIQAUAAYABwAAAPf//f8UABYAEQAWACoALQAvAC0ALQAfABEA/f/1/9v/w//L/97/8v8HABkAKQApABoACQD1/9z/zP/D/8z/9f8OADcASgBIADcAJAAAAOH/0//X/97/9P8JABYACQDy/+P/vv/G/7b/0//n/xQAOwBIAEIAHQD//9v/tv+i/7n/yP/0/xkAMgAtABoADgDs/9D/wP+2/7b/s//M/9n/5v/W/+n/8v/x/+r/9/8DAAQALwBDAD0A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+v/n/+r/5//f/9D/1v/m/+T/6f/x//L///8DAAYA/f/6//L//f/6////+v/9//z/CwAGAAMAAQAEAAYAAQADAAAABgAAAAwABAALAAEAFwASAA8AFgAHABwABAD9/+7/9P/T/9n/5v/f/9v/4f/u/9//+f/u////4f/x/9z/6f/x/+r/8v/1//3/4f/x/9T/5P/B/9T/wP/M/73/wP+7/63/tv+y/8D/tv/B/7b/0P/B/9b/zP/k/9b/5P/X/+H/1P/X/87/3v/e/8v/3P/R/9v/0//Z/97/1v/h/9P////f/wAA8f8PAPf/DgD6/wAA8v/9//L/8f/q/9n/7//k//z/0P/8/+r/+f/j/+f/5P/m/9f/2//c/+H/3P/b/9f/3P/e/9T/6f/f/9//0//f/+P/2f/U/9f/0//T/9P/0f/I/8j/y//M/8v/yP/M/8n/zP/U/+f/4f/s//z/5//5//f/8v8BAAMAAwD///z//f8EAPr/8v/u//L/2f/u/+z/+f/m/+P/BAABAPT//P8JAPX//P/x//f/+v/5////9//3/+z/9P/s/+z/3P/1/+b/7//e//X/8f/s//L/+f8SAAMAAQD8/xYAAQAGAAMADAAGAA4AAwAAAAYA8f8BAPz/AAD9//L/FgAcAPn/AwADABwAFgAMADQALwAsACwALwAaABoADwAcAB0ADwADABoA+f8HAAAACQD3//r/9//5/wEA1v/8/xQAEgDs////AwAUAOb//f/v/w4A7P/3//X/CQDp/+P///8JAPr/5P8EAAEA/f///wsA/f8LAPH/HAAAAPL/7/8HAAEA6v/5//z//f8MAAQAAAAAAAYAGgD9/xcA/P8fAAYAHQADACUACwABABIAEgAOABcABAABABYACQD6//T/HAAPAAsA7v8OAA8AJwDu/wsAGgAJAPr//P8XAA4A9/8EACoADgAHAAMACwD8//n//f/0/+r/+v8AAAYA+f///x0AEQAOAAAACQAXAA8ABAAHAAwADgD5/wwAJAAXAPr/GQAfAAsABgAHAAcADgD8/zIAHAD1/wEALwAiAOf/4/8qABcA3v/I/xQAAAD3/w4A3//8/xEAMgARALX/HQBoADcAjf9G/yUB8v/Z/8j/rf8fAToAjP8S/3MAMwHe/7f+S//UAS0BcP72/pUBKAGi/6z+wgAoAQAAHf9AAJkAYwD9/wEAPQAvAJIA0P/f/0oAlADG/9v/RwDvAPX/sP9IADsBHwCC/zgA7wBsAKL/6f+fANkA9/+2/3wAswAXAP3/2f98AHYA3P8GAB0AVQBZAAAA/P83AHAABwDZ/xcAZgApANP/FgBzACwA0/8PAFsALwDJ/wkAWQAfAPL/5v8MABoA7v/j/wwADwDp/x0A8v/j/y8AAwDZ//f/3/8XAM7/s//x/wYA3/++//L////s/9z/3v/0/wMA1v/9/+z/4f/9//r/7//j/+f//P/m/+P/9P8AAOn/9/8JAAEA5/8HAAkAAADe////IgAUAOz/+f8HAAkA9f/h/xEA/f/5/+//AAAJAAQA8v/8/wEA/f/6//X/7P/0//f/7v/v/+P/6f/p/9//4//c/+H/3P/R/9b/y//B/9P/wP/B/9P/s//J/97/0P/R/8j/yf/Z/7j/u//M/8P/vf+5/7v/0P+2/63/yf/F/63/uP/A/8v/zv+9/+P/7P/j/9f/6f/3//z/9P8DAPf/8v/y/+//8f/3//L/7v/u/+H/7v/Z/9H/0P/T/9b/1P/Z/+P/3P/U/9b/4f/c/9v/4f/j/+f/6f/m/+r/6v/m/9v/1v/X/9T/1v/T/9f/1v/X/9D/zP/Q/8j/wP/M/8n/vf++/8v/3P/Z/9n/4f/k/+z/5v/s/+f/5//s//f/8v/x//T/5//u/+H/7P/q/+//7//q/+n/4//h/+T/7//n/+z/8v8AAAYAAAAPAAYADgAUAP//BwABAPf/AQADAAQABwAOAAcADwALAAkACQABAAEABgAAAA8A+f8AAAYA//8GAAkADAAUAA8AHAAdAB0ALAAnAC0AMgA0ADcALAAtAC0ALAAnACUAJAAiACkALQAlACwAJAApACcAJAAtADQAJQAaABcAKQAcACUAJAAXACQAHQAWAB8AEQAMABwADwAJAAcAAwAGAAYAAAAEAAAAAwADAAMABAD9/wEADwAHAAYADwAGAAcAEQALAA8ADwAOAA8ADwAJAAMA/f8PABIAFgAPABEAFgAZACIALwAqACUALwAsADQAKgAvADUANAAqADUANQAsACoAHAAfAB8AFAAXABQADgASAAsAHAAhABYAGQAWABQADgAPAAkAAAABAAMAAAABAPr//P/9/wAAAQD6//n/+f/s//T/6v/u//f/7//u//z/7P/9/wcAAwAHAAQABgARABIADgAJAAkABwABAAwADAAOAAkAEQAUAAcACwALAAEABwAHAAkABwAMAAcAFwAXAA4ADwAnAB8AGQAMABEAEQAHAA8ADgAMABYADwAMAAMAAAD//wcA9f/9//z//P/x/+T/7//y/+7/6f/u/+P/5v/e/+z/8f/v/+n/7//p//T///8BAAMABwAOAAsADwAMAAMABwAAAAkA/////wEA/f/0//f//P/v//r/9f///wMA//8JABkADgAaABkAJQAlAC0ALQApAC8AIgAkABoAFwAWAAkAHwAUAA4ABAAEAAcADAAOAA8ACwALAA4ABwAMAAkAEQAfAB8ADwAXABEAHQAXABYAGQAUABQACQABAAAA9//y/wwA///0//////8BAPX/8v/0//T/7v/x//X/7v/k/+T/8v/s/+f/7//j/+n/3v/j/+b/5//b/9P/1v/L/9T/0//b/97/1//W/97/1P/W/9v/3v/q/+r/5//k/+z/5v/s/+z/5//q/+7/5//j/9//3P/W/9z/5v/s/+7/7P/v//H/9f////z///8MAAYACQAJAAMAAQD6//r/9f/v/+7/6v/q/+H/1//W/9z/4//X/9P/3v/f/97/6v/k/+7/5v/x//L/9P/m/+P/5v/j/+T/2//f/9n/0f/G/8b/0P+9/8n/vv/G/73/wP/R/9z/0//O/9P/4f/k/+T/7P/x//L////6//f/9f/8/+//+v/0//L/9P/n/9//5v/m/+H/6f/s//L/9f/0//f/+v8HABIAEQAOAA4ACQADAAEAAAD0//L/8f/p/+n/4f/m/+P/3P/f/+P/2f/R/9n/xf/O/9T/5v/u/+r/8f/8//n/7v/q/+b/4//u/+7/7P/u/+b/4f/n/+r/8f/x/+r/6v/k/+n/9P/y//r/BgAMAA4AEgAPABwAHwAWABoAHwAdABYAFgAPAAQAAAAHAPr///8AAP///f8SAA4ADAARACQAHQApACUAKQAiAC0ALwA4ADIAIgAiACUAJAAZABwAHAAcABkAHwAnACQAJAAhACEAGQAiACkAQAA1ADUAOwA7AEIASABIAFAASgBFAEgAQAA7AC8ANAA/AC8ANAA0AD0AQwBFAD8ALwAsACkAKgAnAC0AKgAqADgANwA4ADgAMAAiACEAGgAaAA4AFAASAAcACQD6//3/BwABAAYAAwAEAAYA///8/wEAAwD//wwADwAUAAkADwApACcAJwAcABkAGQAaAB0AGgAfABYAEgAXABoAIgAaADsANAAtAC8ALQAtAC0ALAApACwAKgAlAB8AJAARAB0AIQAaABEADgALAAQAAAD//wYA/f/9//X/9//y/+n/5v/1//L/6f/u/+//7P/q/+b/5//n/+n/5v/s/+7/5v/v//L/8v/u/+n/6v/f/+n/5P/k/+r/6v/0//z//f/y////DAAOAA4AAwAJAA8ACwAJAAYADAAHAAQABwALAAQA+v8OABQAAwAJAAMAAAAAAAEABAAHAA8AAwAGAAYA/P8AAAEA/f/6//L/8v/s/+z/5P/x//X/+v/6//z/9//n/+z/9f/s/+r/5v/n/+r/3P/h/+P/4f/m/+P/5P/p/+f/5P/q//X/7P/1//z//f8DAAMAAAD5//z/AQAGAAEA+v/y//3////0//3/8f8EAA8ADAAHAAMACQD9/xEADwAHABcAHQAsABcAGgAcABcAHQAfACcAHwAkACUAHwAkAB8AHQAyADIAKQAiACQAJAAiACcAKgApACUAHAAcAB0AGgAZACEAHwAWAB0AGQAhAB0AHQAaABkAFwAUAA4ACQD9/wQAHQASABQAFAALAAwABAAGAPz/+v/u/+b/7v/s/+P/3//y//X/+v/u//H/9//u/+z/7v/1//n/+f/v/+7/5//u//H/9P////r/+v/6//X/9//s/+b/5P/1//r/8f/y/+7/+v/9//n//f8AAAYA/f8DAAsABgAHAAQABAAJAAEACQASABYADgAGAAYA///0//H/9P/p/+7/5v/k/+n/5P/q//n/7P/s//H/8f/1/+7/7//p//r/8f/9//z/AAD6//z////0//H/7P/p/+P/5P/f/9z/2//h/9//5//n/+r/8f/9/wEA/P8AAPr/+v/1/wEAAAD8/wEAAAAOAAYAAwAAAA8ADgAGAAAAAwD8/wMA/f8AAAEA/f/6//f/+f8AAAEADgAHAPX/7//v/+z/8v/v//L/8v/m/+b/5v/m/9z/3//v/+z/4//k/9n/3P/f/+H/4//c/+z/8f/9//z/9P/8/wQADAD//wYABgAHAAMACwAPAA4ABgAWABQAFAABAPz/EgAMAAkABwARAAwACwASABkAFAAWABYAHQAfABIAFwAqACQAJwAhAC0A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PwAlABwAOgAyAC8AIgBDADIAOgAyADAAJwAyACUACwD5/w8ADgDx//3/4f////X/HwD//yIACwA6ABQALAAiACcABAALABwAFgDq/+7/AwD8/9v/7v/q//T/7P/k/wQACwAJAP3/IgAdABYABwA0AAQAMAAOACIAFwASAAsA+v8LANn//P/n/+T/u//W/+r/2/+w/9H/2//e/9D/7P/y/+//8v/y//r/9f/y/wwA9//9//T/BADx/wQA4f/5/9H/8v/j//H/4f/v/+//6v/9/w8AHwD5/xkALQAZAAwAFwAPACQABwALACUAGQAAABwAKgAiABYADwApABcA+v8nAAMAAQD3/wkA4f/f//T/GgDh////9/8UAAMADAAWACIAAwAXAAAA+f/3/wEA8f/u/wcA9//k//T/AADm//3/4/8MANP/FAD3/xcAAwAcACkADwAvABkALAAhABcAGQAfABEAGgAsAAYAJAAUACUAIQAlABwAGQAlABIAJQARABQANQAWADIADgA9ACEAHQAtAC0AGgABACcAFAAHAO7/AQDs/+f/3P/3/+H/xf/R//T/0P/O/+b/+f/X/+z/5v8SANv//P8DAPX/9f/u/wwA7//v/xQACwDk/wkA7P8AAPz//f8HAAwABAAqAAsA/P8lAAcACwABAPf/FgD5//f/DAAJAPz/+v8AAPf/5P/j//f/7P/j/9v/1P/u/+P/7P/f/+P/8f/p/+//1P/Q/+n/5P/Q/77/1/+2/87/tf/M/9H/1v/b/+b/3P/n/87/1v/p/+r/8v/k//f/5v/9//T/+f/8//L/BgADAOr/DgDu/+T/0//e/+T/0f++/9b/1//Q/8X/y//R/8v/wf+7/+T/3//Z/87/7P/k/9//0//3/8v/2//W/xIA5v/T/+z/8v/v/9n/JAD9/+T/+v8XAOr/7P/u/xEA4//c/wQA///M//L/BAAMAO//BwAPAAwA9/8OABkA7/8EAA8AAAD5/+7/FgAGAOz//f///+7/+v8AAAYA8v8AAO7//f/j/+P/3v/b/8n/0P/U/9P/wf/F/9D/yf/D/+H/yf/s/8P/5P/M/+r/0f/k/9z/4//T/+z/7P/v/+r/4//1//T/5//u//3/8v///wAACQAEAA4ADgAHAC0AGQAqACIAPwA9ADAAOABNAFUASABHAEUAUgA7AEcANABQADIAQgAqAEUANQAtADcAUgAtACEATgAqADQANQBIADcAQgAsAFIAMgAvACUATgA4ABoAGQAaAAkA+v8fAP3/AwAGABQABwD//w4A/P8hABQAHwAMAAwACQAfAA4ABAAUABoACwAEABoABgAHABEAHwAUABkAEQAWABEAFAAJABEAAQASAA8ABgD8/w4AIQAPAAwABgAJABIADwAMAAkAEgD3/wcA+v/5/+n/4/8DAPz/7//n/+b/8f/p/+z/2f/s//H/4//q//L/5v/0/+//AwDx//L//f8BAOr/8v/q/+n/5//9//H/9f/h/+7/AQDx/wkA+v/6//n/HQAMABkADgAfABQACwAlAAQADAAEABwA9P8aABYALQAnAAwALQAiACcAMAAsACEAFwAaACkAKgAPABQAHwD//w8ABgD1/+f/9//m//T/2//e/9v/7P/D/97/2f/m/9P/6f/k/+//3P/h//n/7/8DAOT/AQAGAAAA9f/0/wEA8v/v/wYA7P/8/+T/7P/u/+n/2//h//n/9f/5//n/8f/1/wQA6f/y//X/7v/5//L/5v/n/9//3P/v/+r/3v/6/+P/+v/p/wAA9f8BAPf/AQD5//H/9f8AAPf/8f8BAPH/7//1//f/6v///wAAAQDy/+7/AAD//wcAEgAlABYAIQAkACoALQAkADAAJQAhADAAMgAXAAsAIQAdAAsAFgAhABoA/f8PAAMACQAHAAkA/f8JAP//CwALABkAEgD8//f/9f8JAOr/7//v//z/0//q//f/3v/X//H/9P/h/9v/AADv/9//9P/6//n/1//0/////P/Z//n/AQDk//f/+v/5/wAABAAPAAkAAAAOABcA///9/wkAAwAOAAcACQAcABEA8v8GAP3/9f///+7///8AAOz////q/wAAAQDu//H/AAADAAMABwABAP3/GQAEAAsAEQAJAA8A9//6//L/+v/Q/97/2f/T/9v/1//x/9n/3//h//f/5P/p//X/BgDp/+b/CwADAOn///8LAA8AAwADAAcAAQD//wwADgD8////DwD6/wAAAwAGAPn//f8GAAAA9f/k/+//+f/0//L/+v/k//r//P8BAN//+v/3/+r/7P/1/+b/5v/b/9f/8f/W/9z/0P/J/87/3//U//z/7P/5/+P/9f/1/xIA+f8JABIAFwARABcADwAHAAYA9f/1//T/7P/m/+//6f/9////+v/0//H/CwD1//n/AwDq/+//+f/8/wkA7v8EABEAAwD///r//P/x//L/7v/0/+f/8f/v//n/1//c//X/8f/k//n/8f/e////+v8GAPr/8v/9//f/+v/0//3/FAD1//r/BAALAAAA9P8GAP3/AQABAAQACwABAAQADgAlACkAMAAaACoALQAwADIAPwBCADAANQBFAEoAQwBAAD8AUwAnAEMAPQA6ACQALwA9ADQAMABFADIAOAAlADoAPQAvACoAPQAwACcAKgAtACUAFAAiACQAGgARAAEAIQAZABEA9f8EAAAA9f8DABEABgADAAEAHQAGAAwAEgAcABwAIgAlABkACQAZAP3///////n/7v/v//f/4f/u/+P/AwAEAPT/EgAEABEABAAdAA8ADgAiAAcAHwASABkACQAdAB8AJAAPACUABwD//wYACwAOAAEA+f/1//z/4f/k/wAA+v/1//T/7//x/97/5P8AAPL/2f/x//L////c/+f/8v/m/9//0//e/+H/4f/1/+z/7v/c/+7/6f/e/+b/1//k/+7/7//1/wcA//8PAAkADwAEAA8AAwD9//z/9//x/+//9P/v//L/9P8GAPL/+v/3/wMAAQD1//H/BwDq/+n/DAAEAOn/3//8/w4AAADu////8v/s/+b/9//y/8z/1//0/9n/1v/Z/9v/0f/O/9z/5v/e/97/2f/h/9P/4//q/97/5P/m/+b/5P/s/9z/0//D/9v/xv/F/8j/xf++/8j/y//f/8n/yP/R/9f/zv/W/9z/3//p/+T/4//v/+7/3//0/+7/6f/s/+n/AwD5//3/+v8AAAAA/P8GAAwACwAWABEAJwAWACQAGgAXAB0AIgAtACoAHAAdACkAHQAZAA8AOgAhABoAMAAcABwAIgAlABwAFwAaACQAHwALABIAIQASABQAJwAfAAQAEgAaABYAAQARABcA/P/v//f/7P/k//H/7P/Z/9z/6v/X//L/7//u//f/9/8DAAAA/f8JAAkADwAAAAwABAAGAP3/AAD//wwACwASABIACQAEAAcABwAHABIAAAALAAwAFAAAABIACQAaABcAFwASAAYA9/8BAAcABwAAAAEABwADAAsADAAhAAwABAASAAQAAwDv//3/CQAGAAQAAwAJAAQACQD6/wEA+f8AAP3/9//6/+f/5v/1/wAA6f/y/wkA+f/8////9P/3//T/8f/s//T/5P/n//X/4//j/+7/6v/s/+//8v/x/wAA9//0/wAABAADAAMAFgARAA4ABgAPAA4AAwASABEABwD8//f//P/0/9z/3//3//L/9f/q//f/7//v//L//f/y//T/6v/3//T/7v/m/wAA7//m//H/8v/1//n/8v/y//f/7P/3//f/+f/n//H/AwAOAAAACQALABoADgAZACoAKgA0AB0ANAAsACwAEgAyADAAHwAhAC8ALAAlACcAJQAlACEADAAiAB8ADgAXACoAKgAUABYAGgAnABcAFgAaABoAGQARACEADgD9//n/FwD9//L/9//1//n/9P/8//n/+f/x//n/AAAEAA4ABwAcAB8ALAAvADcANwA/AEMASABKAFMAUwBKAFIAVQA9AFUASABQADgAOgA6ADUALAAvADUAMgAvAEMASwAvAEUAQwA6ADIAJwAwACQAKgAkACcAGQAqAB0AGgAhABYAFAAwACUAJAApACoAJQAdACEAEQAUABEAEgAGABEABgAHABYAFAAOAAwAHQAWABQAHQAWAB8AFAAaAAwAEgAHAAMAHwAOAAEAAQAAAPH/8f/x/wAA9f8AAPn/9//9//L/AAAGAPr/+v/0//n/+f8GAP//BwAGAAAA/P/6/wQA9f/6//3//P/6/+//8f/0//3/6v/s/+//+v/x//r/AwD0/+///P/5//L/8v/8//3/7//9//T/9P/y/+r/5//k/+b/3//s/9v/0P/W/+H/3//k/+T/4//q/+b/5//c/+H/4f/e/+b/7P/q/+b/5//j/+7/3//s//3/9/8AAPr/AwAHAAYABgAAAPX//P/s////8f/q/+r/4f/q/9f/5v/q/9b/zv/c/9v/1P/W/9f/3P/f/9v/3P/h/97/6f/h/97/zv/R/9v/zP+4/8H/w/+4/7X/vf+1/77/qP+9/8H/u/+w/7n/yP++/77/yP/U/8X/y//T/9f/1v/Q/9f/3v/b/9v/1//e/9v/4//m/+f/6f/x//T//P/y//X/5v/x/+//5P/p/////P/p//f/+f8DAAcA/f8OAA8A//8XABwAFAAMAB0AJQAkAA4ADAARABEAEQAZABEAEgAhABYAFgAcABkAFgApACoAHAAnAB0AHQAkAB8AHAAnACUAJQAnACIAHQAkADAALwAtAC0AMAAqACkAJQAfACQAHwAnACkAKgAZABkAGgAUABEACwALAAQA/P8DAA8AEQAJAA4ABgAGAAAABwAPAAYA///8/////P/5/+n/9//q/+7/9f/1/wAA9f8AAAYAAAD5//T/+f////n/8v/0//z/9P/9//X/9//6//n/AQD///r/9f///wEAAAAEAAQADwARABIADwAXAP3/BAAHABEAAwAGAAcABwALAAcABwAPAAkAAwASABQABgALABkAEQAUABcAIgAXABcAIQAlABkAIQAdACcAHQARAAsAFgAPAAcABAAHAAsABgAGAAAA9P/y/+7/+f/9//L/9P8DAAEAAAABAP3/BgD5//r/+v/3//H/6f/k/+b/5v/c/+b/6f/s/+b/4//m/+n/7P/j/+7/7//p/+7/7v/u//3/AwD5//r//P/8//n//P/0//X//P/5/wEA+f/8/wYAEgAdAB0AFwAhABwAHAAdABoAHAARAAkAAwABAPr/AADu//X/9//y/+z/6v/u/+//8v8EAAQABwAMAAkACQD8/wkABAAEAAEAAwABAAAA9f/y/+r/4f/v/+7/7P/m/+P/9P8GAP3/BAD//wAA+v8HABEAGQAXACIAIQAhACEAIgAnACcALwAnACkAJAAiACIAIQAXAB8AHwAcAB0AFwASAB0ALQAhACIAIQAhABkAGQARAAwACQAPAAwADAADAPf/8f8BAOr/5v/b/9n/y//T/9D/zv/D/9H/3P/T/87/0f/M/9H/xv/U/9T/0P/Z/9f/4f/h/+b/7v/n/+r/3//k/9v/6v/u/+z/7//3/wAAAwAGAAYACQAPABcAHQAlAB0AIQAfACQAIgAkACcAJwAnACEAJAAWABYADgAEAP3/8v/v//z/+v/6//z/+f/3//T/+f/x////AwABAAQAAAD0//n/9f/q/+r/1//b/9f/2//Z/9//2//T/9n/0f/W/8j/0f/c/+P/4f/k/+b/7P/x//f/7v/n/+r/4f/m/+n/5P/c/97/1P/M/8H/wP/B/8v/xv/D/8X/w//L/8P/yf/B/8n/zv/M/8j/wf+5/7j/u/+1/73/uP+4/7L/oP+l/6P/qv+z/7X/sP+y/7j/vf/R/8n/3//Z/+n/7//1/wEAAAABAAcAAwD9//3/AAAAAAMA+f/y//H/7//5//n//P/9//r/AwABAAMABgAJAAsADgAMAAwAEQAJAA4ACQARAA8AAAAMABoACQADAPT/9f/n//T/+f/k/+P/6v/c/+//5v/v//3/9//5/wAAAAD8/wYA///8//3//f/6//z/9f/1//f//f8BAAYADAARABYAHAAaABoAIgAfACEALAApACUAHwAtADUAHwAlACIAGQAZAB0AHAAXAB8AIgAfACUAJAAkAC8AKgAnACoAJwAiAB8AJwAUABcAFwAPABEAEQAMAA8AFgAOAAcABAAMAAwAEgAUACUAKgAqACkAKgApACIAJAA0ACwAKgAkACUAGQAhAB8AJQAcACIAHQAWABIAAQAJACQAHwAlAB8AFAAdACQAGQAiAB8AHQAcABcAEgABAAMACQARAAkABAAAAAcAAwD6/wEABAADAAsACwAPAAkADwAXABEABAADAPz////8//T//P/6//L/7//0//H/7v/q/wAA/f8HAA4AFwAZACIAIQAaACUAKQAlACcALAAsACEAKQAqABoAHAAWABIADAABAP///P/3//r/9//0/+7/9P8GAAcACQADAAEADAAGAAMA9//y/+f/5v/p/+//6v/k/+f/6f/j/+P/7v/s/+z/4//h/9f/6v/p/+n/9f/k//H/8v/8//X/8v/8/wEAAwD9/wYACwADAAYABwASABIAGQApACQAGQAiACIAKgAnACcAHAAiABoAFAARAAsABwAMAB0AHQAUABIACwAOAA8ADwAOAAQACwAPAAYAAQDy/+///f/v/+7/6f/k/9z/3//c/+H/3//q/+z/6v/h/+P/5P/m/+n/7//y//n///8BABEABgAUABYAGQAaACkAGQAcABwAGgAWABcAFAAWABIAGQAaACIAIQAlACEAHwAPAAcAFgAXABQADwAWABoAJAAhACUAFAAOAA4ACQAOAP//+v/9//n/+v/6//n/9P/p/+P/4//f/97/0P/c/+b/1P/j/+P/5P/k/9n/4f/b/9n/0//Q/8b/yf/U/97/2f/X/9f/3v/Z/9f/0P/c/+f/7//p//T/9P/9//r//P/8/+z/9f/6//n/9f/1//L/9//1/wEA//8GAPL/9f8JAAQABAAEAAMABwAAAAMAAwD6//X//f/8//r/+v/v/+T/3v/Z/9n/4f/f/9//3P/h/+r/5//v//T/8v/5//L/+f///wQAAQALAAQA/P/9/wAA+v/u//L//f/5//r/8v/v//T/9//5/wwADAAJAAkAEQARAA8ADgAUAAwACQAGAAcAAwAEAP3/EQALAPz/8v/x/+7/4//j/+T/4//c/9n/1//U/8n/2f/u/+z/6v/m/+n/6f/h/+b/3v/n/+T/6f/n/+T/3//h/97/5v/k/+n/4//h/+H/5P/k/+P/5P/q/+//8f/1//z/DAASAAcABgABAAMABgAJAAMA//8BAPz/AAD5//L/7v/6//f/8v////r//P8BAAkAEQAaABoAJQAnAC0AJAAyADQAKgAqACUAJQAhAB0AFgALAAsABAABAAkAEQAEAA4AGQAaABkAGgAfABkAJwAqADQAOwA9ADsAQwBCADUAMgA3ADAALAAqACkALAAsADIAMAAyACcAJwApAC0AJwAlACwALAAfABwAHAAdAB0AHQAfABoAHQAcABcAFgAGAAEAFgARAAAAAAAAAAEABwAMAAsADwAJAAAADwALAA4AFwAdABcADwAUABcADgAXAB8AIQAhAB8AGgAcABYADgAZACkAJAAlAB0AHAAaABcAFgAcABYAEQAWABEAFAAJAAcAEQAXAA8AIgAkABoAGQAUABkAEQAMAAEAAAD0//H/8v/9//r/+v/5//r//P/1//r/+f/0//L/AwAHAAkAAwAJAAcADgASABQADgAJAA4A///9//n//P8AAAMABAAEAP//DAAGAPX//f8AAAMABgAEAAAAAQD///3/AAD8//T/7P8BAPT/8v/1/+z/6v/h/9v/3v/b/97/0f/O/9b/zv/c/9v/3P/c/+b/5//X/9z/2f/c/9v/4f/U/97/0f/J/9D/2//h/9n/3//p/+n/8f/x//L/8v/u//H/+v8JAAAA//8BAPz/7P/q/+7/6f/v/+f/5//e/+f/5v/s/+z/4f/m/+7/6v/W/9f/zv/T/9f/1//W/97/4//f/97/2f/U/9b/3P/e/9f/3v/f/9D/0P/R/9f/4f/b/9n/2//k/97/5v/n/9//2//f/+H/6f/y//n//f8DAPz//////wcAAAAAAAYABwAHAAMAAQD//wQABgAGAAkAAAD9////AQD5//T/BgALAAwAFAASAAkA/f/6//z/+v/8///////9//X/9P/5//H/9//3/wAA///8/////P/6//T/9//5//f/9P/8/wAA9f/s//f/7//y//L/+v///wcABAAHAP3/BwD9//3/AwADAP3/AwABAAQABwAGAAsAEgAWABYADAAaAAcADwAHAAsAAwAEAAcAAQAHAAEABAABAAAA/P/6//X/8v/3////+f/6//z/+f/1//n/7P/m/9//5P/q/+z/8f/n/+n/+f/6//r/+v/1//X//f/6////AwADAAEA///8//X/+v/5/+z/3//n/+b/7//6//n/9f/0//z/+v/5//f/8f/8/wcACQAHAAQAAAADAAAA//8AAAAA//8DAP3/BAD9/wAACQAPAAMA/f////z/AwAGAAMABgAAAPr/9//8/+H/7v/y//f/7P/1//H/+f/u/+//7v/m/+T/3v/f/9P/1//k/+f/6f/u/+//5//p/+7/7P/u//X/+v8DAAQADgAGAA8AIgAUABIAEgAUABIAEgASAAcACwAMAA8ACQAJAAQA+v8SABYACQAOAAMAAAAOAAwAEQAXABQAFgAXABIAEQAZAB8AHwASABQADwAEAAMABAADAPn///8BAAEABAABAAMAEQAXABkAIQAiACIALAAsADUANAA7ADoAOgA0ACoANAA6ADUALwAwADQANQA3ADcAKQAlAC0ANwA0ADUALAAqADoANwAyADAALwA9AEAAQAA7ADAAOAAwADAAMgAaAB0AKgAaABYADAALAAMAAAAEAAYACQAEAAkADwAWAA8AFgAlABwAFwARAA8AEgAJAAkABAAHABEAGQARABcACwAPAB8AGQAaABoAFwAkAC8ANQAhAB0AFwAWAA8ACwD6//r//f/1//L/+f/5//n////0//X/BAD//wEABwARAPz/CwAEAAcAEgADAAQAAQAAAPX/6f/n/+n/3v/p/+b/3v/b/+z/5v/p/+b/7//v//H/+f8BAA4AFgASABEAFAALAA8ADgAHAAYAAAD6//H//P/u/+n/9P/y//z/AQAAAO7/5//v/+f/5//h/+P/5P/j/+n/5//q/+//7//x/+//4//c/+H/3v/c/9f/0//M/87/yf/I/8X/vf++/8D/w/+9/8H/yP/R/9H/0f/Z/9f/2f/c/+T/5//y//L/8f/s//H/9f8AAPz/8f/s//L/8f/y/+z/7//u//r//f8DAAQA/f8BAAQAEQABAAcABwAHAAkACQALAA8ACQAGAP//9//m/9//6f/h/9n/2f/b/+P/7v/y/+r/6f/u//X/9//y/+f/5//h/+b/8f/y//n/9f/5//f/AQADAAMABgAEABIADAARABYAFAAOABQADwAWABEAFAASABYAGQARABEAEQALABEAJQAdAB8AGQASABEAEQAaABkAEgASAA4ABwAMAPr///8RABEADgAPAAAADwAGAAcABwAWABQADwAOAAsA9/8DAAcACQABAA4AGgAUABQAFAASABEAFwAWABQAEgAAAAYAGQAXABoAIgAnADQANAApACcAKQAiAB8AGQAcAA8AEQAdACQAJAAkAC0AJQApACIAHQAZAAwABgAJAAYA/P/8/xYAEgAMAAQA//8AAAYA/f8HAAcABwD9/wAAAAD1//n/BwAJAPr/7v/q/+r/7//f/+f/2//f/9v/3//e/9T/3P/k/9//3v/f/+b/6v/h/+H/9//u//T/9//3//z//P/6////+v/y//L/+f/9//3/9//6/wEA/f8DAA4ACwAMABYAFwAcABwAGQAcABkAIQAXACUAGgAZAAwAAQAAAAAABAAPAAwA///9//T/9//8//n/BgD///z/8v/s//H/2//q//n/7v/m/+P/5P/n/+r/8f/6//n/+f///wAA9f/s/+//8v/u/+r/3//b/+H/1v/h/9n/5v/k/+z/5P/k/9//5v/v//f/7//u//H/6v/y/+z/9//8//n/9//x//L/1v/b/9n/6v/k//H/3v/h/+P/6f/u//L/8f/x//L/8f/u//H/AAD9/w4ADAADAAAAAAABAAAA+v8AAAQACwAMAP//AQABAAcAAQAXAAkAFgASAAkAAAABAPz/AwALAAYA+f8AAAEACwAEAAsAEQAUABQAHQAdACEAJwAwACcALAAkABoAJAAkAB0ADwARABkAEgAWAAEABgAGAAkABwAOAAYADwAXABoAEQAXACEAIQAnAB0AHAAfACIADwALAA4ABwAHABwAFgALAAcAAQD3//3///8AAAEA+f8BAPz//f/0//r/AwD9//z/AAADAPf/+v/y//3/9f/0/+7/7v/5//T//P8GAP3/AQDv////AQABAAsABAAGAPz/9/////z/8f/6/wAA/P////n//P/y/+//6v/q//T/7//u//H/8v/q/+7/+v///wYA+v8HAAQADAAUAAcACwAMAA4ABgALAAkABwADABIAAQD///z//P/1//r/8v/0//3/AAAHAAEA+f8HACEAHAAUABQABwARAAkACQALAP/////3//z/9P/k/+H/5v/m/87/0//T/9f/2f/Z/9//6f/5//n/BgAMAAMADgAOAAwABgAJAAcABwALAP//+v/1//X/7P/q/+z/3v/b/+f/5P/b//L//P/9//r//f8JAAkADAAWAA8ADAAEAAAAAAABAAMAAQD5/+7/8f/j/+n/4f/j//H/BgD8/wEABgAUAA8ADwASABkAHAAPABYAGQAXABkAFwAaAA4ADAAMACIAJAAiACQALQA0ADoANwBKAD8ASABKAFkAVgBIAE4AZQBQAEUASgBHAEoAOAAyAC8AKQAwACkALwApAB0AKQA0ADUALQA0ADgANAA4ADoANwA0ACwAJAAWAA8AAQADABcABwAMAAkAAwAAAAYAAwAJABEAEgAXABcAHAAWACIAMgAtACQAKQApAC8ALAA=" type="audio/wav" />
        Your browser does not support the audio element.
    </audio>




.. code:: ipython3

    core = ov.Core()
    
    compiled_quantized_model = core.compile_model(model=quantized_model, device_name='CPU')
    
    input_data = np.expand_dims(test_sample["array"], axis=0)

Next, make a prediction.

.. code:: ipython3

    predictions = compiled_quantized_model([input_data])[0]
    predicted_ids = np.argmax(predictions, axis=-1)
    transcription = processor.batch_decode(torch.from_numpy(predicted_ids))
    transcription




.. parsed-literal::

    ['A MAN SAID TO THE UNIVERSE SIR I EXIST']



Compare Accuracy of the Original and Quantized Models 
###############################################################################################################################

-  Define dataloader for test dataset.
-  Define functions to get inference for PyTorch and OpenVINO models.
-  Define functions to compute Word Error Rate.

.. code:: ipython3

    # inference function for pytorch
    def torch_infer(model, sample):
        logits = model(torch.Tensor(sample['input_values'])).logits
        # take argmax and decode
        predicted_ids = torch.argmax(logits, dim=-1)
        transcription = processor.batch_decode(predicted_ids)
        return transcription
    
    
    # inference function for openvino
    def ov_infer(model, sample):
        output = model.output(0)
        logits = model(np.array(sample['input_values']))[output]
        predicted_ids = np.argmax(logits, axis=-1)
        transcription = processor.batch_decode(torch.from_numpy(predicted_ids))
        return transcription
    
    
    def compute_wer(dataset, model, infer_fn):
        wer = WordErrorRate()
        for sample in tqdm(dataset):
            # run infer function on sample
            transcription = infer_fn(model, sample)
            # update metric on sample result
            wer.update(transcription, [sample['text']])
        # finalize metric calculation
        result = wer.compute()
        return result

Now, compute WER for the original PyTorch model and quantized model.

.. code:: ipython3

    pt_result = compute_wer(dataset, torch_model, torch_infer)
    quantized_result = compute_wer(dataset, compiled_quantized_model, ov_infer)
    
    print(f'[PyTorch]   Word Error Rate: {pt_result:.4f}')
    print(f'[Quantized OpenVino]  Word Error Rate: {quantized_result:.4f}')



.. parsed-literal::

      0%|          | 0/73 [00:00<?, ?it/s]



.. parsed-literal::

      0%|          | 0/73 [00:00<?, ?it/s]


.. parsed-literal::

    [PyTorch]   Word Error Rate: 0.0530
    [Quantized OpenVino]  Word Error Rate: 0.0609

