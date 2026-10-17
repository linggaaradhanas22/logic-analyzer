# HardwareX Article Template — Markdown Edition

Source: `HardwareX Template_20210707.docx` (HardwareX article template Version 2, June 2021).
This Markdown file preserves the template structure and its italic instructions so the
content can be authored here and pasted into the Word template.

> Instructions in *italics* are template guidance and must be deleted before submission.

---

## {Article title}

*Please avoid acronyms and abbreviations where possible.*

## Authors

*List all authors. Please mark the corresponding author with (*).*

## Affiliations

*Please include the full address of each author institution.*

## Corresponding author's email address and Twitter handle

*Institutional email address preferred. If you have a Twitter handle, please add it here "twitter: @....".*

## Abstract

*Max. 200 words. Remember that the abstract is what readers see first in electronic abstracting & indexing services — make it brief, specific, interesting and easy to understand. If a research article refers to your hardware, cite that research article here.*

## Keywords

*Add at least 3 keywords and a maximum of 6 keywords.*

## Specifications table

*Please replace the italicized instructions in the right column of the table with the relevant information about your hardware.*

| Hardware name | The name of the hardware that you have invented/customized |
|---|---|
| Subject area | *Please select the subject area that best reflects the community for which your hardware was developed (deleting the rest):* Engineering and materials science / Chemistry and biochemistry / Medical (e.g., pharmaceutical science) / Neuroscience / Biological sciences (e.g., microbiology and biochemistry) / Environmental, planetary and agricultural sciences / Educational tools and open source alternatives to existing infrastructure / General |
| Hardware type | *Please select the category that best reflects the purpose for which your hardware was developed (deleting the rest):* Imaging tools / Measuring physical properties and in-lab sensors / Biological sample handling and preparation / Field measurements and sensors / Electrical engineering and computer science / Mechanical engineering and materials science / Other [please specify] |
| Closest commercial analog | *Please specify the closest commercial analog to your submitted hardware. In other words, what would this hardware replace? If no commercial analog exists, state "No commercial analog is available."* |
| Open source license | *All designs must be submitted under an open source license (for more details see the Guide for Authors). Please specify the open source license you've selected here.* |
| Cost of hardware | *Insert an approximate cost only — we will ask you for a complete breakdown of costs in a later section (Bill of materials).* |
| Source file repository | *If you've uploaded your source files to an approved repository (OSF, Mendeley Data or Zenodo) write the DOI URL here. For example: http://doi.org/10.17605/OSF.IO/WGK7Q* |
| OSHWA certification UID (OPTIONAL) | *We encourage you to apply for a free OSHWA Certification, which confirms your work is open-source compliant. If certification has been acquired, insert the OSHWA UID here. For example: "CH000005". In your OSHWA certification project description, include a link to your HardwareX publication and the tag #HX. If you haven't acquired certification, please delete this row of the specifications table.* |

## Hardware in context

*Write a short description of the hardware and provide context, i.e., describe similar open hardware and proprietary equipment in the field.*

## Hardware description

*Describe your hardware, highlighting the customization rather than the steps involved in the procedure. Explain how it differs from other hardware and the advantages it offers over pre-existing methods. For example, how does this hardware compare to other hardware in terms of cost or ease of use, or how can it be used to develop further designs in a particular area?*

*Add 3–5 bullet points which broadly explain to other researchers — inside or outside of the original user community — how the hardware could help them, with either standard or novel laboratory tasks.*

### Design files

*Your design files should be editable — see OSHWA's open source definition of "Documentation" for further details. You must then either:*

- *Upload your design files to one of the three approved online repositories — Mendeley Data (instructions), the Open Science Framework (instructions) or Zenodo (instructions). We recommend this option as the repositories support versioning of files.*
- *Upload your design files as supplementary materials (e.g., CAD files, videos) to HardwareX's online editorial system when you submit your manuscript.*
- *Include your design files in the body of the manuscript (e.g., as figures).*

*CAD files: You are encouraged to use free and open source software packages for creating the files. For CAD files, OpenSCAD, FreeCAD, or Blender are encouraged, but, if these are not available, we accept source files from proprietary CAD packages, such as Autocad or Solidworks, and other drawing packages.*

*3D printing. Supplementary files that facilitate digital replication of the devices are encouraged; for example, STL files for 3D printing components. We recommend uploading CAD files to the NIH 3D Print Exchange as Custom Labware and then entering the link here.*

*Electronics: PCB layouts and other electronics design files can be uploaded to the Open Hardware Repository or other repositories or as supplementary materials.*

*Software and firmware: All software files used in the design and operation of the hardware should be included in the repository. Provide a description of the software and firmware and use extensive comments in the code.*

### Design files summary

*Complete a separate row for each design file associated with your hardware (including the primary design files). Any empty rows should be deleted.*

| Design file name | File type | Open source license | Location of the file |
|---|---|---|---|
| *For example: Design file 1* | *e.g., CAD files, figures, videos* | *All designs must be submitted under an open hardware license. Enter the corresponding open source license for the file.* | *Either enter the URL for the repository or the sentence: "Available with the article".* |
|  |  |  |  |
|  |  |  |  |
|  |  |  |  |
|  |  |  |  |
|  |  |  |  |

*For each design file listed in the summary table above, include a short description of the file below (just one or two sentences per design file).*

## Bill of materials

*If your bill of materials is long or complex, you can upload the details in an editable spreadsheet, e.g., ODS file type, Excel spreadsheet or PDF file, to an open access online location, such as the Open Science Framework repository. Include the link here. Alternatively, the bill of materials can be submitted alongside your manuscript as supplementary material.*

### Bill of materials summary

*Complete a separate row for each component of your hardware — all components associated with a cost should be listed and any empty rows should be deleted.*

| Designator | Component | Number | Cost per unit — currency | Total cost — currency | Source of materials | Material type |
|---|---|---|---|---|---|---|
| *If possible, use the same designator here as you use in the associated design file. If that's not possible, you will need to explain the relationship between the two.* | *Name of Component 1* | *Number of units* | *Cost per unit and the currency used* | *Total cost and the currency used* | *If possible, include a direct link to a webpage where the component can be purchased* | *Select from: Metal / Semi-conductor / Ceramic / Polymer / Biomaterial / Organic / Inorganic / Composite / Nanomaterial / Semiconductor / Non-specific / Other* |
|  |  |  |  |  |  |  |
|  |  |  |  |  |  |  |
|  |  |  |  |  |  |  |
|  |  |  |  |  |  |  |

*You can use this space for any additional descriptions of the materials used.*

## Build instructions

*Provide detailed, step-by-step construction instructions for the submitted hardware:*

- *Include all necessary information for reproducing it.*
- *Explain and (when possible) characterize design decisions. Include any design alternatives you created.*
- *Use visual instructions such as schematics, images and videos.*
- *Clearly reference design files and component parts described in the Design file summary and the Bill of materials summary.*
- *Highlight any potential safety concerns.*

## Operation instructions

*Provide detailed, step-by-step instructions for the safe and proper operation of the hardware.*

- *Use visual instructions, as necessary.*
- *Highlight any potential safety hazards.*

## Validation and characterization

*Demonstrate the operation of the hardware and characterize its performance for a specific scientific application.*

- *Highlight a relevant use case.*
- *If possible, characterize performance of the hardware over operational parameters.*
- *Create a bulleted list describing the capabilities (and limitations) of the hardware. For example, load and operation time, spin speed, coefficient of variation, accuracy, precision, etc.*

## Ethics statements

*HardwareX has ethical guidelines that all authors must comply with. In addition, we ask you to complete the relevant statement(s) below. Please delete those which are not relevant for your work.*

*If your work involved human subjects, please include a statement here confirming that the relevant informed consent was obtained from those subjects:*

*If your work involved animal experiments, please include a statement here confirming that those experiments complied with the ARRIVE guidelines and were carried out in accordance with the U.K. Animals (Scientific Procedures) Act, 1986 and associated guidelines; EU Directive 2010/63/EU for animal experiments; or the National Institutes of Health guide for the care and use of laboratory animals (NIH Publications No. 8023, revised 1978). Note, the sex of the animals must be indicated, and, where appropriate, the influence (or association) of sex on the results of the study:*

## CRediT author statement

*CRediT is an initiative that enables authors to share an accurate and detailed description of their diverse contributions to a published work.*

*Example of a CRediT author statement: Zhang San: Conceptualization, Methodology, Software. Priya Singh: Data curation, Writing — Original draft preparation. Wang Wu: Visualization, Investigation. Jan Jansen: Supervision. Ajay Kumar: Software, Validation. Sun Qi: Writing — Reviewing and Editing.*

*Please add a CRediT author statement for your data article here, using the categories listed on this webpage.*

## Acknowledgments

*All contributors who do not meet the criteria for authorship should be listed in an acknowledgments section.*

*In addition, please list any funding sources in this section. List funding sources in this standard way to facilitate compliance to funder's requirements:*

*Funding: This work was supported by the National Institutes of Health [grant numbers xxxx, yyyy]; the Bill & Melinda Gates Foundation, Seattle, WA [grant number zzzz]; and the United States Institutes of Peace [grant number aaaa].*

*It is not necessary to include detailed descriptions on the program or type of grants and awards. When funding is from a block grant or other resources available to a university, college, or other research institution, submit the name of the institute or organization that provided the funding.*

*If no funding has been provided for the research, please include the following sentence:*

*This research did not receive any specific grant from funding agencies in the public, commercial, or not-for-profit sectors.*

## References

*If relevant, you should include a reference to the original publication of the hardware you customized and a reference to the repository in which your design files are published. Other references can be included, as required; for example, references that put your device in context in the literature. For more information on the reference format in HardwareX please see the Guide for Authors.*

---

## Additional Information for authors (do not include these lines in your submission)

### Author manuscript checklist

- Is the subject of the submission under an open source license? Are design files in the preferred format for making modifications as defined by the Open Source Hardware definition?
- Can the hardware be reproduced with the details provided in the submission?
- Are all relevant design files available on either the Mendeley Data, Open Science Framework, or Zenodo repository? Are they described in the Design Files Summary, and clearly documented? (E.g., descriptive file names, commented code, labeled images, etc.)
  - If in the Open Science Framework, has the repository been registered? Instructions.
  - If in Zenodo, is the repository open access and published? Instructions.
  - If in Mendeley Data, is the repository published or the sharable link included in the additional information you plan to submit? Instructions.
- Are visual instructions used when necessary?
- Is the utility of the hardware to the scientific community explained clearly? Has a specific scientific application been demonstrated using the hardware?
- Is the performance of the hardware adequately demonstrated and characterized?
- Are all potential safety concerns addressed?
- For more information on the article template consult the Guide to Authors.

*Reminder: Before you submit, please delete all the instructions in this document (the text in italics), including this paragraph. Thank you!*
