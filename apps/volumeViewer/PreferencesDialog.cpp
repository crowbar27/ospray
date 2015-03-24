// ======================================================================== //
// Copyright 2009-2015 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "PreferencesDialog.h"
#include "VolumeViewer.h"

PreferencesDialog::PreferencesDialog(VolumeViewer *volumeViewer) : QDialog(volumeViewer), volumeViewer(volumeViewer)
{
  setWindowTitle("Preferences");

  QFormLayout *formLayout = new QFormLayout();
  setLayout(formLayout);

  // gradient shading flag
  QCheckBox *gradientShadingEnabledCheckBox = new QCheckBox();
  connect(gradientShadingEnabledCheckBox, SIGNAL(toggled(bool)), volumeViewer, SLOT(setGradientShadingEnabled(bool)));
  formLayout->addRow("Volume gradient shading", gradientShadingEnabledCheckBox);

  // sampling rate selection
  QDoubleSpinBox *samplingRateSpinBox = new QDoubleSpinBox();
  samplingRateSpinBox->setDecimals(3);
  samplingRateSpinBox->setRange(0.01, 1.0);
  samplingRateSpinBox->setSingleStep(0.01);
  connect(samplingRateSpinBox, SIGNAL(valueChanged(double)), volumeViewer, SLOT(setSamplingRate(double)));
  formLayout->addRow("Sampling rate", samplingRateSpinBox);

  // volume clipping box
  for(size_t i=0; i<6; i++) {
    volumeClippingBoxSpinBoxes.push_back(new QDoubleSpinBox());

    volumeClippingBoxSpinBoxes[i]->setDecimals(3);
    volumeClippingBoxSpinBoxes[i]->setRange(0.0, 1.0);
    volumeClippingBoxSpinBoxes[i]->setSingleStep(0.01);

    if(i < 3)
      volumeClippingBoxSpinBoxes[i]->setValue(0.);
    else
      volumeClippingBoxSpinBoxes[i]->setValue(1.);

    connect(volumeClippingBoxSpinBoxes[i], SIGNAL(valueChanged(double)), this, SLOT(updateVolumeClippingBox()));
  }

  QWidget *volumeClippingBoxLowerWidget = new QWidget();
  QHBoxLayout *hBoxLayout = new QHBoxLayout();
  volumeClippingBoxLowerWidget->setLayout(hBoxLayout);

  hBoxLayout->addWidget(volumeClippingBoxSpinBoxes[0]);
  hBoxLayout->addWidget(volumeClippingBoxSpinBoxes[1]);
  hBoxLayout->addWidget(volumeClippingBoxSpinBoxes[2]);

  formLayout->addRow("Volume clipping box: lower", volumeClippingBoxLowerWidget);

  QWidget *volumeClippingBoxUpperWidget = new QWidget();
  hBoxLayout = new QHBoxLayout();
  volumeClippingBoxUpperWidget->setLayout(hBoxLayout);

  hBoxLayout->addWidget(volumeClippingBoxSpinBoxes[3]);
  hBoxLayout->addWidget(volumeClippingBoxSpinBoxes[4]);
  hBoxLayout->addWidget(volumeClippingBoxSpinBoxes[5]);

  formLayout->addRow("Volume clipping box: upper", volumeClippingBoxUpperWidget);

  // set default values. this will trigger signal / slot executions.
  gradientShadingEnabledCheckBox->setChecked(false);
  samplingRateSpinBox->setValue(0.125);
}

void PreferencesDialog::updateVolumeClippingBox()
{
  osp::vec3f lower(volumeClippingBoxSpinBoxes[0]->value(), volumeClippingBoxSpinBoxes[1]->value(), volumeClippingBoxSpinBoxes[2]->value());
  osp::vec3f upper(volumeClippingBoxSpinBoxes[3]->value(), volumeClippingBoxSpinBoxes[4]->value(), volumeClippingBoxSpinBoxes[5]->value());

  volumeViewer->setVolumeClippingBox(osp::box3f(lower, upper));
}
