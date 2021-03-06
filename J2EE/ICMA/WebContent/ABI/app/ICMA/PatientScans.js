/*******************************************************************************
 *  Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 *  The contents of this file are subject to the Mozilla Public License
 *  Version 1.1 (the "License"); you may not use this file except in
 *  compliance with the License. You may obtain a copy of the License at
 *  http://www.mozilla.org/MPL/
 *
 *  Software distributed under the License is distributed on an "AS IS"
 *  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 *  License for the specific language governing rights and limitations
 *  under the License.
 *
 *  The Original Code is ICMA
 *
 *  The Initial Developer of the Original Code is University of Auckland,
 *  Auckland, New Zealand.
 *  Copyright (C) 2011-2014 by the University of Auckland.
 *  All Rights Reserved.
 *
 *  Contributor(s): Jagir R. Hussan
 *
 *  Alternatively, the contents of this file may be used under the terms of
 *  either the GNU General Public License Version 2 or later (the "GPL"), or
 *  the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 *  in which case the provisions of the GPL or the LGPL are applicable instead
 *  of those above. If you wish to allow use of your version of this file only
 *  under the terms of either the GPL or the LGPL, and not to allow others to
 *  use your version of this file under the terms of the MPL, indicate your
 *  decision by deleting the provisions above and replace them with the notice
 *  and other provisions required by the GPL or the LGPL. If you do not delete
 *  the provisions above, a recipient may use your version of this file under
 *  the terms of any one of the MPL, the GPL or the LGPL.
 *
 *
 *******************************************************************************/
define([
	"dojo/_base/declare",
	"dojo/_base/lang",
	"dojo/aspect",
	"dojo/date/locale",
	"dojo/topic",
	"dijit/layout/ContentPane",
	"dijit/layout/TabContainer",
	"ABI/app/ICMA/Scan"
], function(declare, lang, aspect, locale, topic, ContentPane, TabContainer, Scan){

	// module:
	//		ABI/app/ICMA/PatientScans

	return declare("ABI.app.ICMA.PatientScans", [ContentPane],{
		// summary:
		//		A widget that manages views of Scans
		// description:
		//		
		patientManager: null,
		server: null,
		
		namedEcho: 0,
	
		constructor: function(params){
			dojo.mixin(this, params);
			
			aspect.after(this.patientManager, "onPatientUpdate", lang.hitch(this, this.update));
			
			topic.subscribe("/link", lang.hitch(this, this.selectStudy));
		},
		
		startup: function(){
			if(this._started){ return; }
			this.inherited(arguments);
		},
		
		buildRendering: function(){
			this.inherited(arguments);

			this.tcScans = new TabContainer({
	            style: "height: 100%; width: 100%;"
	        });
	        
	        this.tcScans.startup();
	        
	        this.tcScans.watch("selectedChildWidget", function(name,oval,nval){
	        	//console.debug("Calling from selectedChildWidget ",nval);
	        	topic.publish("/loadFirstUSView",nval);
	        });
	        
	        this.domNode.appendChild( this.tcScans.domNode);
		},
		
		update: function(){
			var patient = this.patientManager.getCurrentPatient();
			//console.dir(patient);

            var children = this.tcScans.getChildren();
            if(children.length > 0){
            	for(var i = 0; i < children.length; i++){
            		this.tcScans.removeChild(children[i]);
            	}
            }

			for(item in patient.usstudies){
				var study = patient.usstudies[item];
				var sxfit = this.server + "/" + patient.SXFIT ;
				var sxspeckletrack = this.server + "/" + patient.SXSPECKLETRACK;
				this.addScan(study,sxfit,sxspeckletrack);
			}
			
			// fix duplicate names
			var children = this.tcScans.getChildren();
			if(children.length>0){
				for(var i = 1; i < children.length; i++){
					if(children[i].title == children[i-1].title){
	                    if(!children[i-1].titleNumber){
	                    	children[i-1].titleNumber = 1;
	                    }
	                    children[i].titleNumber = children[i-1].titleNumber + 1;
					}
	            }
				for(var i = 0; i < children.length; i++){
					 if(children[i].titleNumber){
				         children[i].set("title", children[i].title + " (" + children[i].titleNumber + ")");
					 }
				}
	/*			if(children.length ==0){
					this.NoScansMsg = new ContentPane({
					      content:"<h2>There are no Ultra sound scans in the database</h2>",
					    });
					this.tcScans.addChild(this.NoScansMsg);
				}*/
				// Set services urls
				//ABI.app.ICMA.MultiImageFitSingleton.setSxFit(this.server + "/" + patient.SXFIT);
				//ABI.app.ICMA.MultiImageFitSingleton.setSxSpeckleTrack(this.server + "/" + patient.SXSPECKLETRACK);
				this.tcScans.selectChild(children[0]);
				topic.publish("ABI/lib/Widgets/ustabalive");
			}else{
				topic.publish("ABI/lib/Widgets/ustabhide");
			}
			topic.publish("ABI/lib/Widgets/scansready");
		},
		
		addScan: function(study,sxfit,sxspeckletrack){
			var title = study.name ? study.name : "Echo";
			
	        var scan = new Scan({
	             study: study,
	             title: title,
	      	     SXFIT: sxfit,
	             SXSPECKLETRACK: sxspeckletrack,
	             server: this.server,
	        });


            // Need to find the insert index
            var index = 0;
            var scaneDateTime = this.getScanDateTime(scan);       
            var children = this.tcScans.getChildren();
            for(var i = 0; i < children.length; i++){
            	var dateTime = this.getScanDateTime(children[i]);
            	//if(dateTime > scaneDateTime){
            	if(dateTime < scaneDateTime){
            		index++;
            	}
            }
	        this.tcScans.addChild(scan, index);
		},
		
		getScanDateTime: function(scan){
			    var dateTokens = scan.study.studyDate.split(" "); //eg "12-10-2000 10:44:30.0000"
                dateTokens[1] = dateTokens[1].replace(/\./g, ":"); // replace . with : in the time field
    
                var studyDate = locale.parse(dateTokens[0],{
                    selector: "date",
                    datePattern: "dd-MM-yyyy",
                }); 
    
                var studyTime = locale.parse(dateTokens[1],{
                    selector: "time",
                    timePattern: "KK:mm:ss:SSSS",
                }); 
                
                return datetime = new Date(studyDate.getFullYear(), studyDate.getMonth(), studyDate.getDate(), studyTime.getHours(), studyTime.getMinutes(), studyTime.getSeconds());
		},
		
		resize: function(){
			this.inherited(arguments);
		},
		
		selectStudy: function(study){
            var children = this.tcScans.getChildren();
            for(var i = 0; i < children.length; i++){
                //if(children[i].study.studyUid == study){
            	if(children[i].study.studyUID.indexOf(study)>-1){
                	this.tcScans.selectChild(children[i]);
                }
            }
            //Check in MRI tab
        }
	

	});
});
