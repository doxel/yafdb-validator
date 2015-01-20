#include "ymlparser.h"
#include <QDebug>

YMLParser::YMLParser()
{
}


void YMLParser::writeItem(cv::FileStorage &fs, ObjectRect* obj)
{
    // Write class name
    switch(obj->getObjectType())
    {
    case ObjectType::Face:
        fs << "className" << "Face";
        break;
    case ObjectType::NumberPlate:
        fs << "className" << "NumberPlate";
        break;
    case ObjectType::ToBlur:
        fs << "className" << "ToBlur";
        break;
    case ObjectType::None:
        fs << "className" << "None";
        break;
    }

    // Write class name
    switch(obj->getSubType())
    {
    case ObjectSubType::None:
        fs << "subClassName" << "None";
        break;
    case ObjectSubType::Front:
        fs << "subClassName" << "Front";
        break;
    case ObjectSubType::Profile:
        fs << "subClassName" << "Profile";
        break;
    case ObjectSubType::Back:
        fs << "subClassName" << "Back";
        break;
    case ObjectSubType::Top:
        fs << "subClassName" << "Top";
        break;
    case ObjectSubType::Eyes:
        fs << "subClassName" << "Eyes";
        break;
    }

    // Write square area coodinates
    fs << "area" << "{";
        fs << "p1" << cv::Point2d(obj->proj_point_1().x(), obj->proj_point_1().y());
        fs << "p2" << cv::Point2d(obj->proj_point_2().x(), obj->proj_point_2().y());
        fs << "p3" << cv::Point2d(obj->proj_point_3().x(), obj->proj_point_3().y());
        fs << "p4" << cv::Point2d(obj->proj_point_4().x(), obj->proj_point_4().y());
    fs << "}";

    // Write params
    fs << "params" << "{";
        fs << "azimuth" << obj->proj_azimuth();
        fs << "elevation" << obj->proj_elevation();
        fs << "aperture" << obj->proj_aperture();
        fs << "width" << obj->proj_width();
        fs << "height" << obj->proj_height();
    fs << "}";

    // Write status tags
    fs << "autoStatus" << obj->getAutomaticStatus().toStdString();
    fs << "manualStatus" << obj->getManualStatus().toStdString();
    fs << "blurObject" << (obj->isBlurred() ? "Yes" : "No");
}

ObjectRect* YMLParser::readItem(cv::FileNodeIterator iterator, int ymltype)
{
    // Initialize detected object
    ObjectRect* object = new ObjectRect;

    // Parse falsePositive tag
    std::string falsePositive;
    (*iterator)["falsePositive"] >> falsePositive;
    QString lowerFalsePositive = QString( falsePositive.c_str() ).toLower();

    // Parse class name
    std::string className;
    (*iterator)["className"] >> className;

    // Parse sub class name
    std::string subClassName;
    (*iterator)["subClassName"] >> subClassName;

    QString lowerClassName = QString( className.c_str() ).toLower();
    QString lowerSubClassName = QString( subClassName.c_str() ).toLower();

    if(lowerClassName == "face")
    {
        object->setObjectType( ObjectType::Face );
    } else if (lowerClassName == "front" || lowerClassName == "front:profile"){
        object->setObjectType( ObjectType::Face );
        object->setSubType( ObjectSubType::Front );
    } else if (lowerClassName == "profile"){
        object->setObjectType( ObjectType::Face );
        object->setSubType( ObjectSubType::Profile );
    } else if(lowerClassName == "numberplate") {
        object->setObjectType( ObjectType::NumberPlate );
    } else if(lowerClassName == "toblur") {
        object->setObjectType( ObjectType::ToBlur );
    } else if(lowerClassName == "none") {
        object->setObjectType( ObjectType::None );
    }

    if(lowerSubClassName == "none")
    {
        object->setSubType( ObjectSubType::None );
    } else if(lowerSubClassName == "front") {
        object->setSubType( ObjectSubType::Front );
    } else if(lowerSubClassName == "profile") {
        object->setSubType( ObjectSubType::Profile );
    } else if(lowerSubClassName == "back") {
        object->setSubType( ObjectSubType::Back );
    } else if(lowerSubClassName == "top") {
        object->setSubType( ObjectSubType::Top );
    } else if(lowerSubClassName == "eyes") {
        object->setSubType( ObjectSubType::Eyes );
    }

    // Parse area points
    cv::FileNode areaNode = (*iterator)["area"];
    cv::Point2d pt_1;
    cv::Point2d pt_2;
    cv::Point2d pt_3;
    cv::Point2d pt_4;

    switch(ymltype)
    {
    case YMLType::Detector:
        areaNode["p1"] >> pt_1;
        areaNode["p2"] >> pt_3;
        break;
    case YMLType::Validator:
        areaNode["p1"] >> pt_1;
        areaNode["p2"] >> pt_2;
        areaNode["p3"] >> pt_3;
        areaNode["p4"] >> pt_4;
        break;
    }

    object->setPoints(QPointF(pt_1.x, pt_1.y),
                      QPointF(pt_2.x, pt_2.y),
                      QPointF(pt_3.x, pt_3.y),
                      QPointF(pt_4.x, pt_4.y));

    // Parse gnomonic params
    cv::FileNode paramsNode = (*iterator)["params"];
    float azimuth = 0.0;
    float elevation = 0.0;
    float aperture = 0.0;
    float width = 0.0;
    float height = 0.0;

    paramsNode["azimuth"] >> azimuth;
    paramsNode["elevation"] >> elevation;
    paramsNode["aperture"] >> aperture;
    paramsNode["width"] >> width;
    paramsNode["height"] >> height;

    object->setProjectionParametters(azimuth,
                                    elevation,
                                    aperture,
                                    width,
                                    height);

    object->setProjectionParametters(azimuth, elevation, aperture, width, height);
    object->setProjectionPoints();

    // Parse auto status
    std::string autoStatus;
    (*iterator)["autoStatus"] >> autoStatus;

    QString lowerAutoStatus = QString( autoStatus.c_str() ).toLower();
    lowerAutoStatus = lowerAutoStatus.length() > 0 ? lowerAutoStatus : "none";

    if(lowerAutoStatus != "none")
    {
        if(lowerAutoStatus == "valid")
        {
            object->setObjectAutomaticState( ObjectAutomaticState::Valid );
            object->setAutomaticStatus( "Valid" );

            if( ymltype == YMLType::Detector )
            object->setBlurred( true );

        } else {
            object->setObjectAutomaticState( ObjectAutomaticState::Invalid );

            switch(ymltype)
            {
            case YMLType::Detector:
                if( lowerAutoStatus == "filtered-ratio" )
                {
                   object->setAutomaticStatus( "Ratio" );
                } else if( lowerAutoStatus == "filtered-size" ){
                   object->setAutomaticStatus( "Size" );
                } else if( lowerAutoStatus == "filtered-ratio-size" ){
                    object->setAutomaticStatus( "Ratio-Size" );
                }
                break;
            case YMLType::Validator:
                if( lowerAutoStatus == "ratio" )
                {
                   object->setAutomaticStatus( "Ratio" );
                } else if( lowerAutoStatus == "size" ){
                   object->setAutomaticStatus( "Size" );
                } else if( lowerAutoStatus == "ratio-size" ){
                    object->setAutomaticStatus( "Ratio-Size" );
                } else if( lowerAutoStatus == "missingoption" ){
                    object->setAutomaticStatus( "MissingOption" );
                }
                break;
            }
        }
    } else {
        object->setObjectAutomaticState( ObjectAutomaticState::Manual );
    }

    object->setAutomaticStatus( (object->getAutomaticStatus().length() > 0 ? object->getAutomaticStatus() : "None") );

    // Parse manual status
    std::string manualStatus;
    (*iterator)["manualStatus"] >> manualStatus;
    object->setManualStatus( QString(manualStatus.c_str()) );
    object->setManualStatus( (object->getManualStatus().length() > 0 ? object->getManualStatus() : "None") );

    if(lowerFalsePositive == "no")
    {
        if(lowerAutoStatus == "none")
        {
            object->setManualStatus("Valid");
        }
    } else if( lowerFalsePositive == "yes" )
    {
        object->setManualStatus("Invalid");
    }

    if(object->getObjectType() == ObjectType::ToBlur)
    {
        object->setObjectManualState( ObjectManualState::ToBlur );
    } else {

        if(object->getManualStatus() != "None")
        {
            if(object->getManualStatus() == "Valid")
            {
                object->setObjectManualState( ObjectManualState::Valid );
            } else {
                object->setObjectManualState( ObjectManualState::Invalid );
            }
        } else {
            object->setObjectManualState( ObjectManualState::None );
        }

    }

    if( ymltype == YMLType::Validator )
    {
        std::string blurObject;
        (*iterator)["blurObject"] >> blurObject;

        QString blurObject_lower = QString( blurObject.c_str() ).toLower();

        object->setBlurred( blurObject_lower == "yes" ? true : false );
    }

    cv::FileNode childNode = (*iterator)["childrens"];
    for (cv::FileNodeIterator child = childNode.begin(); child != childNode.end(); ++child) {
        object->childrens.append( this->readItem( child ) );
    }

    object->setResizeEnabled( false );

    // Return object
    return object;
}

void YMLParser::writeYML(QList<ObjectRect*> objects, QString path)
{
    // Open storage for writing
    cv::FileStorage fs(path.toStdString(), cv::FileStorage::WRITE);

    // Write source file path
    fs << "source_image" << objects.first()->getSourceImagePath().toStdString();

    // Write objects
    fs << "objects" << "[";

    // Iterate over objects
    foreach (ObjectRect* obj, objects) {

        // Open array element
        fs << "{";

        // Object writer method
        this->writeItem(fs, obj);

        // Write childrens if present
        if(obj->childrens.length() > 0)
        {
            fs << "childrens" << "[";
            foreach (ObjectRect* child, obj->childrens) {
                fs << "{";
                this->writeItem(fs, child);
                fs << "}";
            }
            fs << "]";
        }

        // Close array element
        fs << "}";
    }

    // Close array
    fs << "]";
}

QList<ObjectRect*> YMLParser::loadYML(QString path, int ymltype)
{
    // Init output list
    QList<ObjectRect*> out_list;

    // Read YML file
    cv::FileStorage fs(path.toStdString(), cv::FileStorage::READ);

    // Retrieve objects node
    cv::FileNode objectsNode = fs["objects"];

    // Retrieve invalid objects node
    cv::FileNode invalidObjectsNode = fs["invalidObjects"];

    // Iterate over objects
    for (cv::FileNodeIterator it = objectsNode.begin(); it != objectsNode.end(); ++it) {

        // Initialize detected object
        ObjectRect* object = this->readItem(it, ymltype);

        std::string source_image;
        fs["source_image"] >> source_image;

        object->setSourceImagePath( QString(source_image.c_str()) );

        // Append to list
        out_list.append(object);
    }

    // Iterate over objects
    for (cv::FileNodeIterator it = invalidObjectsNode.begin(); it != invalidObjectsNode.end(); ++it) {

        // Initialize detected object
        ObjectRect* object = this->readItem(it, ymltype);

        object->setObjectAutomaticState( ObjectAutomaticState::Invalid );
        object->setAutomaticStatus( (object->getAutomaticStatus().length() <= 0 || object->getAutomaticStatus() == "None") ? "MissingOption" : object->getAutomaticStatus() );

        std::string source_image;
        fs["source_image"] >> source_image;

        object->setSourceImagePath( QString(source_image.c_str()) );

        // Append to list
        out_list.append(object);
    }

    // Return results
    return out_list;
}

