import { NgModule } from '@angular/core';
import { BrowserModule } from '@angular/platform-browser';
import { AppComponent } from './app.component';
import { BrowserAnimationsModule } from '@angular/platform-browser/animations';
import { FormsModule } from '@angular/forms';

import {MatLegacyButtonModule as MatButtonModule} from '@angular/material/legacy-button';
import {MatLegacyInputModule as MatInputModule} from '@angular/material/legacy-input';
import {MatLegacyTabsModule as MatTabsModule} from '@angular/material/legacy-tabs';
import {MatLegacyCardModule as MatCardModule} from '@angular/material/legacy-card';
import {MatDividerModule} from '@angular/material/divider';
import {MatLegacySliderModule as MatSliderModule} from '@angular/material/legacy-slider';
import {MatGridListModule} from '@angular/material/grid-list';
import {MatCommonModule} from '@angular/material/core';
import {MatLegacySlideToggleModule as MatSlideToggleModule} from '@angular/material/legacy-slide-toggle';



@NgModule({
  declarations: [
    AppComponent
  ],
  imports: [
    MatCommonModule,
    FormsModule,
    BrowserModule,
    BrowserAnimationsModule,
    MatButtonModule,
    MatInputModule,
    MatTabsModule,
    MatCardModule,
    MatDividerModule,
    MatSliderModule,
    MatGridListModule,
    MatSlideToggleModule
  ],
  providers: [],
  bootstrap: [AppComponent]
})
export class AppModule { }
